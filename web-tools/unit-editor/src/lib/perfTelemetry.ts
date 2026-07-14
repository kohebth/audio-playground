export type PerfMarkerMeta = Record<string, string | number | boolean>;

export type PerfSpan = {
  name: string;
  durationMs: number;
  startedAt: number;
  endedAt: number;
  at: number;
  meta?: PerfMarkerMeta;
};

const MAX_SAMPLES = 120;
const AUTOSAVE_DEBOUNCE_MS = 350;
const TRACE_WINDOW = '__apgPerfTrace';

type PerfStore = {
  enabled: boolean;
  samples: PerfSpan[];
  maxSamples: number;
};

declare global {
  interface Window {
    [TRACE_WINDOW]?: PerfStore;
  }
}

let spanCounter = 0;

function getStore(): PerfStore {
  if (typeof window === 'undefined') {
    return { enabled: false, samples: [], maxSamples: MAX_SAMPLES };
  }

  let store = window[TRACE_WINDOW];
  if (!store) {
    store = { enabled: import.meta.env.DEV, samples: [], maxSamples: MAX_SAMPLES };
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

  performance.mark(start);
  try {
    return action();
  } finally {
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
  }
}

export function readPerfSpans(limit = 10): PerfSpan[] {
  const samples = getStore().samples;
  if (limit <= 0) return [...samples];
  return samples.slice(-limit);
}

export function clearPerfSpans(): void {
  const store = getStore();
  store.samples = [];
}

export const PERFORMANCE_DEBOUNCE_MS = AUTOSAVE_DEBOUNCE_MS;
