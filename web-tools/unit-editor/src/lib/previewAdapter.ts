import type { RenderResult } from './backendSamples';

export type PreviewState = 'idle' | 'ready' | 'running' | 'error';

export type PreviewMeters = {
  peak: number;
  rms: number;
  frames: number;
};

export type PreviewAdapter = {
  compile: () => PreviewState;
  start: () => PreviewState;
  stop: () => PreviewState;
  setParam: (path: string, value: string) => string;
  setBypass: (instanceId: string, enabled: boolean) => string;
  getMeters: () => PreviewMeters;
};

export function createDeterministicPreviewAdapter(render: RenderResult): PreviewAdapter {
  return {
    compile: () => (render.ok ? 'ready' : 'error'),
    start: () => (render.ok ? 'running' : 'error'),
    stop: () => (render.ok ? 'ready' : 'error'),
    setParam: (path, value) => `setParam ${path}=${value}`,
    setBypass: (instanceId, enabled) => `setBypass ${instanceId}=${enabled}`,
    getMeters: () => ({
      peak: render.output.peak,
      rms: render.output.rms,
      frames: render.frames,
    }),
  };
}
