import type { AudioTraceReport, AudioTraceSnapshot, AudioTraceStageName } from '@audio-playground/wasm-tools';

export type AudioTraceBrowserLatency = AudioTraceReport['browser'];

export const AUDIO_TRACE_STAGE_LABELS: Record<AudioTraceStageName, string> = {
  schedulingJitter: 'Callback cadence gap',
  inputCopy: 'Input copy',
  wasmProcess: 'WASM graph',
  outputCopy: 'Output copy',
  latencyProbe: 'Latency probe',
  channelCopy: 'Channel copy',
  callbackTotal: 'Total callback',
};

export function formatAudioTraceBudget(stage: AudioTraceStageName, utilization: number): string {
  return stage === 'schedulingJitter' ? 'n/a' : `${utilization.toFixed(1)}%`;
}

const INTERNAL_STAGES = [
  'inputCopy',
  'wasmProcess',
  'outputCopy',
  'latencyProbe',
  'channelCopy',
] as const;

export function createAudioTraceReport(
  trace: AudioTraceSnapshot,
  browser: AudioTraceBrowserLatency,
  capturedAt = new Date().toISOString(),
): AudioTraceReport {
  const slowestInternalStage = trace.sampleCount === 0
    ? null
    : INTERNAL_STAGES.reduce((slowest, stage) => (
      trace.stages[stage].p95Ms > trace.stages[slowest].p95Ms ? stage : slowest
    ));
  const overBudget = trace.callbackDeadlineMissesDelta > 0
    || (trace.deadlineMs > 0 && trace.stages.callbackTotal.maxMs > trace.deadlineMs);
  const verdict = overBudget ? 'internal-over-budget' : 'internal-healthy';

  return {
    schema: 'apg.audio-trace.v2',
    capturedAt,
    browser,
    trace,
    slowestInternalStage,
    verdict,
    message: verdict === 'internal-over-budget'
      ? `Internal processing exceeded the ${trace.deadlineMs.toFixed(3)} ms callback deadline.`
      : 'Callback execution remained within budget. Cadence gaps are diagnostic only; remaining delay is in browser or device buffering.',
  };
}

export function exportAudioTraceReport(report: AudioTraceReport): void {
  const blob = new Blob([`${JSON.stringify(report, null, 2)}\n`], { type: 'application/json' });
  const url = URL.createObjectURL(blob);
  const link = document.createElement('a');
  link.href = url;
  link.download = `apg-audio-trace-${report.capturedAt.replace(/[:.]/g, '-')}.json`;
  link.click();
  URL.revokeObjectURL(url);
}
