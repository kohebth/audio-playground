export type PerfMarkerMeta = Record<string, string | number | boolean>;

export type PerfSpan = {
  name: string;
  durationMs: number;
  startedAt: number;
  endedAt: number;
  at: number;
  meta?: PerfMarkerMeta;
};
export type PerfRenderSpan = {
  id: string;
  phase: 'mount' | 'render' | 'update' | string;
  actualDurationMs: number;
  baseDurationMs: number;
  startTime: number;
  commitTime: number;
  at: number;
};

const MAX_SAMPLES = 240;
const MAX_RENDER_SAMPLES = 240;
const AUTOSAVE_DEBOUNCE_MS = 350;
const TRACE_WINDOW = '__apgPerfTrace';

type PerfStore = {
  enabled: boolean;
  samples: PerfSpan[];
  maxSamples: number;
  renderSamples: PerfRenderSpan[];
  maxRenderSamples: number;
  componentRenders: Record<string, number>;
};

declare global {
  interface Window {
    [TRACE_WINDOW]?: PerfStore;
  }
}

let spanCounter = 0;

function getStore(): PerfStore {
  if (typeof window === 'undefined') {
    return {
      enabled: false,
      samples: [],
      maxSamples: MAX_SAMPLES,
      renderSamples: [],
      maxRenderSamples: MAX_RENDER_SAMPLES,
      componentRenders: {},
    };
  }

  let store = window[TRACE_WINDOW];
  if (!store) {
    store = {
      enabled: import.meta.env.DEV,
      samples: [],
      maxSamples: MAX_SAMPLES,
      renderSamples: [],
      maxRenderSamples: MAX_RENDER_SAMPLES,
      componentRenders: {},
    };
    window[TRACE_WINDOW] = store;
  }

  return store;
}

export function isPerfTracingEnabled(): boolean {
  return import.meta.env.DEV && getStore().enabled;
}

export function markPerfSpan<T>(name: string, action: () => T, meta?: PerfMarkerMeta): T {
  const store = getStore();
  if (!import.meta.env.DEV || !store.enabled || typeof performance === 'undefined') {
    return action();
  }

  const markerId = `${name}-${spanCounter++}`;
  const start = `${markerId}:start`;
  const end = `${markerId}:end`;
  const startedAt = performance.now();

  const finish = () => {
    const endedAt = performance.now();
    const durationMs = endedAt - startedAt;

    performance.mark(end);
    performance.measure(name, start, end);
    const sample: PerfSpan = { name, durationMs, startedAt, endedAt, at: Date.now(), meta };
    store.samples.push(sample);
    if (store.samples.length > store.maxSamples) store.samples.shift();

    performance.clearMarks(start);
    performance.clearMarks(end);
    performance.clearMeasures(name);
  };

  performance.mark(start);
  try {
    const result = action();
    if (result instanceof Promise) {
      return result.finally(finish) as T;
    }
    finish();
    return result;
  } catch (error) {
    finish();
    throw error;
  }
}

export function readPerfSpans(limit = 10): PerfSpan[] {
  const samples = getStore().samples;
  if (limit <= 0) return [...samples];
  return samples.slice(-limit);
}

export function readPerfRenderSpans(limit = 10): PerfRenderSpan[] {
  const samples = getStore().renderSamples;
  if (limit <= 0) return [...samples];
  return samples.slice(-limit);
}

export function markRenderPerfSpan(span: PerfRenderSpan): void {
  if (!import.meta.env.DEV || !getStore().enabled || typeof performance === 'undefined') return;
  const store = getStore();
  store.renderSamples.push(span);
  if (store.renderSamples.length > store.maxRenderSamples) store.renderSamples.shift();
}

export function clearPerfRenderSpans(): void {
  const store = getStore();
  store.renderSamples = [];
}

export function markComponentRender(component: string, id: string): void {
  if (!import.meta.env.DEV) return;
  const store = getStore();
  store.componentRenders ??= {};
  const key = `${component}:${id}`;
  store.componentRenders[key] = (store.componentRenders[key] ?? 0) + 1;
}

export function clearPerfComponentRenders(): void {
  getStore().componentRenders = {};
}

export function clearPerfSpans(): void {
  const store = getStore();
  store.samples = [];
}

export const PERFORMANCE_DEBOUNCE_MS = AUTOSAVE_DEBOUNCE_MS;
