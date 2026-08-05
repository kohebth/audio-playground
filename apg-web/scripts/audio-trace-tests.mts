import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { createContext, runInContext } from 'node:vm';
import { resolve } from 'node:path';

import { createAudioTraceReport, formatAudioTraceBudget } from '../src/lib/audioTrace.ts';
import type { AudioTraceSnapshot, AudioTraceStageStats } from '@audio-playground/apg-wasm';

const emptyStage = (): AudioTraceStageStats => ({
  sampleCount: 3,
  meanMs: 0.1,
  p95Ms: 0.2,
  maxMs: 0.3,
  deadlineUtilization: 7.5,
});

const snapshot: AudioTraceSnapshot = {
  status: 'complete',
  sampleRate: 48_000,
  quantumFrames: 128,
  deadlineMs: 128 / 48,
  elapsedMs: 5_000,
  durationMs: 5_000,
  callbackCount: 1_875,
  sampleCount: 235,
  underrunsDelta: 0,
  callbackDeadlineMissesDelta: 0,
  stages: {
    schedulingJitter: emptyStage(),
    inputCopy: emptyStage(),
    wasmProcess: { ...emptyStage(), p95Ms: 0.8, maxMs: 1.1 },
    outputCopy: emptyStage(),
    latencyProbe: emptyStage(),
    channelCopy: emptyStage(),
    callbackTotal: { ...emptyStage(), p95Ms: 1.2, maxMs: 1.4 },
  },
};

const report = createAudioTraceReport(snapshot, {
  captureLatencyMs: 4,
  baseLatencyMs: 2.7,
  outputLatencyMs: 8,
  acousticLoopbackMs: 22,
}, '2026-07-16T00:00:00.000Z');
assert.equal(report.schema, 'apg.audio-trace.v2');
assert.equal(report.verdict, 'internal-healthy');
assert.equal(report.slowestInternalStage, 'wasmProcess');
assert.match(report.message, /Cadence gaps are diagnostic only/);
assert.equal(JSON.parse(JSON.stringify(report)).trace.sampleCount, 235);

const overBudget = createAudioTraceReport({
  ...snapshot,
  callbackDeadlineMissesDelta: 1,
}, report.browser);
assert.equal(overBudget.verdict, 'internal-over-budget');

const schedulingDelayed = createAudioTraceReport({
  ...snapshot,
  stages: {
    ...snapshot.stages,
    schedulingJitter: { ...emptyStage(), p95Ms: 4, maxMs: 5 },
  },
}, report.browser);
assert.equal(schedulingDelayed.verdict, 'internal-healthy');
assert.match(schedulingDelayed.message, /Cadence gaps are diagnostic only/);
assert.equal(formatAudioTraceBudget('schedulingJitter', 312.5), 'n/a');
assert.equal(formatAudioTraceBudget('callbackTotal', 37.5), '37.5%');

const source = readFileSync(resolve('../apg-wasm/web/processor.worklet.js'), 'utf8')
  .replace(/^import createApgProcessorModule[^\n]*\n/, '');
let clockMs = 0;
let Processor: new (options: unknown) => {
  ready: boolean;
  handle: (request: unknown) => Promise<void>;
  process: (inputs: Float32Array[][], outputs: Float32Array[][]) => boolean;
  port: { messages: unknown[] };
};

class MockAudioWorkletProcessor {
  port = {
    messages: [] as unknown[],
    onmessage: null as ((event: { data: unknown }) => void) | null,
    postMessage: (message: unknown) => this.port.messages.push(message),
  };
}

const heap = new Float32Array(2_048);
const sandbox = {
  Array,
  ArrayBuffer,
  BigInt,
  Date,
  Float32Array,
  Float64Array,
  Math,
  Promise,
  Uint8Array,
  AudioWorkletProcessor: MockAudioWorkletProcessor,
  currentFrame: 0,
  sampleRate: 48_000,
  performance: { now: () => (clockMs += 0.01) },
  createApgProcessorModule: async () => ({
    HEAPF32: heap,
    HEAPU32: new Uint32Array(heap.buffer),
    _apg_wasm_processor_create: () => 1,
    _apg_wasm_processor_frame_capacity: () => 128,
    _apg_wasm_processor_input_buffer: () => 0,
    _apg_wasm_processor_output_buffer: () => 1_024 * 4,
    _apg_wasm_processor_process: (_processor: number, frames: number) => {
      heap.copyWithin(1_024, 0, frames);
      return 0;
    },
  }),
  registerProcessor: (_name: string, implementation: typeof Processor) => {
    Processor = implementation;
  },
};
const context = createContext(sandbox);
runInContext(source, context);

const stats = runInContext('traceStageStats(new Float64Array([1, 2, 3, 4, 100]), 5, 10)', context);
assert.deepEqual(
  { sampleCount: stats.sampleCount, meanMs: stats.meanMs, p95Ms: stats.p95Ms, maxMs: stats.maxMs },
  { sampleCount: 5, meanMs: 22, p95Ms: 100, maxMs: 100 },
);

const processor = new Processor({ processorOptions: { moduleUrl: 'apg_processor.mjs', wasmBinary: new ArrayBuffer(1) } });
await new Promise(resolvePromise => setTimeout(resolvePromise, 0));
assert.equal(processor.ready, true);
await processor.handle({ id: 1, type: 'startAudioTrace' });

const input = new Float32Array(128);
const output = new Float32Array(128);
for (let callback = 0; callback < 1_875; callback += 1) {
  clockMs += 128 / 48;
  processor.process([[input]], [[output]]);
  sandbox.currentFrame += 128;
}
await processor.handle({ id: 2, type: 'pollAudioTrace' });
const response = processor.port.messages.at(-1) as { trace: AudioTraceSnapshot };
assert.equal(response.trace.status, 'complete');
assert.equal(response.trace.callbackCount, 1_875);
assert.equal(response.trace.sampleCount, 235);
assert(response.trace.sampleCount <= 512);
assert.equal(response.trace.stages.schedulingJitter.sampleCount, 512);
assert.equal(response.trace.stages.callbackTotal.sampleCount, 235);
for (const stage of Object.values(response.trace.stages)) {
  assert(Number.isFinite(stage.meanMs));
  assert(stage.meanMs <= stage.p95Ms);
  assert(stage.p95Ms <= stage.maxMs);
}

await processor.handle({ id: 3, type: 'startAudioTrace' });
processor.process([[input]], [[output]]);
await processor.handle({ id: 4, type: 'pollAudioTrace' });
const resetResponse = processor.port.messages.at(-1) as { trace: AudioTraceSnapshot };
assert.equal(resetResponse.trace.status, 'running');
assert.equal(resetResponse.trace.callbackCount, 1);
assert.equal(resetResponse.trace.sampleCount, 1);
assert.equal(resetResponse.trace.stages.schedulingJitter.sampleCount, 1);
assert.equal(resetResponse.trace.underrunsDelta, 0);
assert.equal(resetResponse.trace.callbackDeadlineMissesDelta, 0);

console.log('audio trace tests passed');
