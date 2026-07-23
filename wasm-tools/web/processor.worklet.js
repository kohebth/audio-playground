import createApgProcessorModule from './apg_processor.mjs';

const AUDIO_TRACE_DURATION_SECONDS = 5;
const AUDIO_TRACE_SAMPLE_EVERY = 8;
const AUDIO_TRACE_SAMPLE_CAPACITY = 512;
const AUDIO_TRACE_STAGE_NAMES = [
  "schedulingJitter",
  "inputCopy",
  "wasmProcess",
  "outputCopy",
  "latencyProbe",
  "channelCopy",
  "callbackTotal",
];

function monotonicNow() {
  return globalThis.performance?.now ? globalThis.performance.now() : Date.now();
}

function emptyTraceStageStats() {
  return { sampleCount: 0, meanMs: 0, p95Ms: 0, maxMs: 0, deadlineUtilization: 0 };
}

function traceStageStats(samples, count, deadlineMs) {
  if (count === 0) return emptyTraceStageStats();
  const sorted = Array.from(samples.subarray(0, count)).sort((a, b) => a - b);
  const meanMs = sorted.reduce((sum, value) => sum + value, 0) / count;
  const p95Ms = sorted[Math.max(0, Math.ceil(count * 0.95) - 1)];
  const maxMs = sorted[count - 1];
  return {
    sampleCount: count,
    meanMs,
    p95Ms,
    maxMs,
    deadlineUtilization: deadlineMs > 0 ? (p95Ms / deadlineMs) * 100 : 0,
  };
}

function resolveSibling(path, base) {
  if (/^[a-z]+:/i.test(path)) return path;
  const directory = base.slice(0, base.lastIndexOf('/') + 1);
  return path === '.' ? directory : `${directory}${path}`;
}

if (!globalThis.URL) {
  globalThis.URL = class WorkletUrl {
    constructor(path, base) {
      this.href = resolveSibling(path, base);
    }
  };
}
class ApgWasmProcessor extends AudioWorkletProcessor {
  constructor(options) {
    super(options);
    this.module = null;
    this.processor = 0;
    this.ready = false;
    this.underruns = 0;
    this.callbackDeadlineMisses = 0;
    this.maxCallbackMs = 0;
    this.latencyProbe = null;
    this.audioTrace = null;
    this.port.onmessage = (event) => void this.handle(event.data);
    const { moduleUrl, wasmBinary } = options?.processorOptions ?? {};
    if (!moduleUrl || !(wasmBinary instanceof ArrayBuffer)) {
      this.reply({ id: 0, ok: false, type: "error", message: "Processor module URL or WASM bytes are missing" });
      return;
    }
    void this.initialize(moduleUrl, wasmBinary);
  }
  async initialize(moduleUrl, wasmBinary) {
    try {
      this.module = await createApgProcessorModule({
        locateFile: (file) => resolveSibling(file, moduleUrl),
        wasmBinary,
      });
      this.processor = this.module._apg_wasm_processor_create();
      if (!this.processor) throw new Error("Could not create WASM processor handle");
      this.ready = true;
      this.reply({ id: 0, ok: true, type: "initialized" });
    } catch (error) {
      this.reply({ id: 0, ok: false, type: "error", message: error instanceof Error ? error.message : String(error) });
    }
  }
  reply(response) {
    this.port.postMessage(response);
  }
  command(id, status) {
    this.reply(
      status === 0 ? { id, ok: true, type: "command" } : { id, ok: false, type: "error", message: `Processor command failed with status ${status}` }
    );
  }
  async handle(request) {
    if (!this.module || !this.processor || !this.ready) {
      this.reply({ id: request.id, ok: false, type: "error", message: "Processor is not initialized" });
      return;
    }
    if (request.type === "stage") {
      const bytes = new Uint8Array(request.image);
      const pointer = this.module._malloc(bytes.byteLength);
      if (!pointer) {
        this.reply({ id: request.id, ok: false, type: "error", message: "Processor image allocation failed" });
        return;
      }
      this.module.HEAPU8.set(bytes, pointer);
      const status = this.module._apg_wasm_processor_stage_image(this.processor, pointer, bytes.byteLength);
      this.module._free(pointer);
      this.reply(
        status === 0 ? { id: request.id, ok: true, type: "staged", revision: request.revision } : { id: request.id, ok: false, type: "error", message: `Runtime stage failed with status ${status}` }
      );
      return;
    }
    if (request.type === "commit") {
      const status = this.module._apg_wasm_processor_commit_staged(this.processor, BigInt(request.revision));
      this.reply(
        status === 0 ? { id: request.id, ok: true, type: "committed", revision: request.revision } : { id: request.id, ok: false, type: "error", message: `Runtime commit failed with status ${status}` }
      );
      return;
    }
    if (request.type === "setParam") {
      this.command(request.id, this.module._apg_wasm_processor_set_param(this.processor, request.index, request.value));
      return;
    }
    if (request.type === "setBypass") {
      this.command(
        request.id,
        this.module._apg_wasm_processor_set_bypass(this.processor, request.index, request.enabled ? 1 : 0)
      );
      return;
    }
    if (request.type === "reset") {
      this.command(request.id, this.module._apg_wasm_processor_reset(this.processor));
      return;
    }
    if (request.type === "startLatencyProbe") {
      if (this.latencyProbe) {
        this.reply({ id: request.id, ok: false, type: "error", message: "A latency probe is already running" });
        return;
      }
      this.latencyProbe = {
        id: request.id,
        startFrame: currentFrame + Math.round(sampleRate * 0.08),
        emittedFrame: -1,
        deadlineFrame: currentFrame + Math.round(sampleRate * 1.5),
        toneFrame: 0,
        correlationSin: 0,
        correlationCos: 0,
        correlationFrames: 0,
      };
      return;
    }
    if (request.type === "startAudioTrace") {
      const stages = {};
      for (const name of AUDIO_TRACE_STAGE_NAMES) stages[name] = new Float64Array(AUDIO_TRACE_SAMPLE_CAPACITY);
      this.audioTrace = {
        status: "running",
        startFrame: currentFrame,
        endFrame: currentFrame + Math.round(sampleRate * AUDIO_TRACE_DURATION_SECONDS),
        callbackCount: 0,
        sampleCount: 0,
        cadenceSampleCount: 0,
        cadenceWriteIndex: 0,
        quantumFrames: 0,
        previousCallbackAt: -1,
        previousFrames: 0,
        underrunsAtStart: this.underruns,
        deadlineMissesAtStart: this.callbackDeadlineMisses,
        stages,
      };
      this.reply({ id: request.id, ok: true, type: "audioTraceStarted" });
      return;
    }
    if (request.type === "pollAudioTrace") {
      this.reply({ id: request.id, ok: true, type: "audioTrace", trace: this.snapshotAudioTrace() });
      return;
    }
    if (request.type === "pollMeters") {
      const meter = this.module._apg_wasm_processor_output_meter(this.processor);
      if (!meter) {
        this.reply({ id: request.id, ok: false, type: "error", message: "Processor meter is unavailable" });
        return;
      }
      const floatWord = meter >>> 2;
      this.reply({
        id: request.id,
        ok: true,
        type: "meter",
        meter: {
          peak: this.module.HEAPF32[floatWord] ?? 0,
          rms: this.module.HEAPF32[floatWord + 1] ?? 0,
          frames: this.module.HEAPU32[floatWord + 2] ?? 0,
          valid: (this.module.HEAPU32[floatWord + 3] ?? 0) !== 0,
          activeRevision: Number(this.module._apg_wasm_processor_active_revision(this.processor)),
          underruns: this.underruns,
          callbackDeadlineMisses: this.callbackDeadlineMisses,
          maxCallbackMs: this.maxCallbackMs,
        },
      });
      return;
    }
    this.module._apg_wasm_processor_destroy(this.processor);
    this.processor = 0;
    this.ready = false;
    this.reply({ id: request.id, ok: true, type: "disposed" });
  }
  processLatencyProbe(input, output) {
    const probe = this.latencyProbe;
    if (!probe) return;
    const frequency = 2000;
    const toneFrames = Math.round(sampleRate * 0.025);
    const correlationWindow = 64;
    for (let frame = 0; frame < output.length; frame += 1) {
      const absoluteFrame = currentFrame + frame;
      if (absoluteFrame >= probe.startFrame && probe.toneFrame < toneFrames) {
        if (probe.emittedFrame < 0) probe.emittedFrame = absoluteFrame;
        output[frame] += 0.2 * Math.sin((2 * Math.PI * frequency * probe.toneFrame) / sampleRate);
        probe.toneFrame += 1;
      }
      if (input && probe.emittedFrame >= 0 && absoluteFrame >= probe.emittedFrame) {
        const phase = (2 * Math.PI * frequency * absoluteFrame) / sampleRate;
        probe.correlationSin += input[frame] * Math.sin(phase);
        probe.correlationCos += input[frame] * Math.cos(phase);
        probe.correlationFrames += 1;
        if (probe.correlationFrames === correlationWindow) {
          const level = (2 * Math.hypot(probe.correlationSin, probe.correlationCos)) / correlationWindow;
          if (level >= 0.035) {
            const detectedFrame = absoluteFrame - Math.floor(correlationWindow / 2);
            this.reply({ id: probe.id, ok: true, type: "latencyProbe", frames: detectedFrame - probe.emittedFrame, sampleRate });
            this.latencyProbe = null;
            return;
          }
          probe.correlationSin = 0;
          probe.correlationCos = 0;
          probe.correlationFrames = 0;
        }
      }
      if (absoluteFrame >= probe.deadlineFrame) {
        this.reply({ id: probe.id, ok: false, type: "error", message: "Latency chirp was not detected by the microphone" });
        this.latencyProbe = null;
        return;
      }
    }
  }
  snapshotAudioTrace() {
    const trace = this.audioTrace;
    const quantumFrames = trace?.quantumFrames ?? 0;
    const deadlineMs = quantumFrames > 0 ? (quantumFrames / sampleRate) * 1000 : 0;
    const stages = {};
    for (const name of AUDIO_TRACE_STAGE_NAMES) {
      const sampleCount = name === "schedulingJitter" ? trace?.cadenceSampleCount : trace?.sampleCount;
      stages[name] = trace ? traceStageStats(trace.stages[name], sampleCount, deadlineMs) : emptyTraceStageStats();
    }
    return {
      status: trace?.status ?? "idle",
      sampleRate,
      quantumFrames,
      deadlineMs,
      elapsedMs: trace ? Math.min(AUDIO_TRACE_DURATION_SECONDS * 1000, Math.max(0, ((currentFrame - trace.startFrame) / sampleRate) * 1000)) : 0,
      durationMs: AUDIO_TRACE_DURATION_SECONDS * 1000,
      callbackCount: trace?.callbackCount ?? 0,
      sampleCount: trace?.sampleCount ?? 0,
      underrunsDelta: trace ? this.underruns - trace.underrunsAtStart : 0,
      callbackDeadlineMissesDelta: trace ? this.callbackDeadlineMisses - trace.deadlineMissesAtStart : 0,
      stages,
    };
  }
  recordCallbackTiming(startedAt, endedAt, frames, traceSampleIndex) {
    const elapsedMs = Math.max(0, endedAt - startedAt);
    this.maxCallbackMs = Math.max(this.maxCallbackMs, elapsedMs);
    if (elapsedMs > (frames / sampleRate) * 1000) this.callbackDeadlineMisses += 1;
    if (traceSampleIndex >= 0 && this.audioTrace) this.audioTrace.stages.callbackTotal[traceSampleIndex] = elapsedMs;
    if (this.audioTrace?.status === "running" && currentFrame + frames >= this.audioTrace.endFrame) {
      this.audioTrace.status = "complete";
    }
  }
  process(inputs, outputs) {
    const output = outputs[0]?.[0];
    if (!output) return true;
    if (!this.module || !this.processor || !this.ready) {
      output.fill(0);
      return true;
    }
    const startedAt = monotonicNow();
    const frames = output.length;
    const trace = this.audioTrace;
    let traceSampleIndex = -1;
    if (trace?.status === "running") {
      trace.callbackCount += 1;
      trace.quantumFrames = frames;
      trace.stages.schedulingJitter[trace.cadenceWriteIndex] = trace.previousCallbackAt < 0
        ? 0
        : Math.max(0, startedAt - trace.previousCallbackAt - (trace.previousFrames / sampleRate) * 1000);
      trace.cadenceWriteIndex = (trace.cadenceWriteIndex + 1) % AUDIO_TRACE_SAMPLE_CAPACITY;
      trace.cadenceSampleCount = Math.min(trace.cadenceSampleCount + 1, AUDIO_TRACE_SAMPLE_CAPACITY);
      if ((trace.callbackCount - 1) % AUDIO_TRACE_SAMPLE_EVERY === 0 && trace.sampleCount < AUDIO_TRACE_SAMPLE_CAPACITY) {
        traceSampleIndex = trace.sampleCount;
        trace.sampleCount += 1;
      }
      trace.previousCallbackAt = startedAt;
      trace.previousFrames = frames;
    }
    if (frames > this.module._apg_wasm_processor_frame_capacity(this.processor)) {
      this.underruns += 1;
      output.fill(0);
      this.recordCallbackTiming(startedAt, monotonicNow(), frames, traceSampleIndex);
      return true;
    }
    const inputCopyStartedAt = traceSampleIndex >= 0 ? monotonicNow() : 0;
    const inputPointer = this.module._apg_wasm_processor_input_buffer(this.processor) >>> 2;
    const input = inputs[0]?.[0];
    if (input) this.module.HEAPF32.set(input, inputPointer);
    else this.module.HEAPF32.fill(0, inputPointer, inputPointer + frames);
    const wasmStartedAt = traceSampleIndex >= 0 ? monotonicNow() : 0;
    if (traceSampleIndex >= 0 && trace) trace.stages.inputCopy[traceSampleIndex] = wasmStartedAt - inputCopyStartedAt;
    const status = this.module._apg_wasm_processor_process(this.processor, frames);
    const outputCopyStartedAt = traceSampleIndex >= 0 ? monotonicNow() : 0;
    if (traceSampleIndex >= 0 && trace) trace.stages.wasmProcess[traceSampleIndex] = outputCopyStartedAt - wasmStartedAt;
    if (status !== 0) {
      this.underruns += 1;
      output.fill(0);
      this.recordCallbackTiming(startedAt, monotonicNow(), frames, traceSampleIndex);
      return true;
    }
    const outputPointer = this.module._apg_wasm_processor_output_buffer(this.processor) >>> 2;
    for (let frame = 0; frame < frames; frame += 1) output[frame] = this.module.HEAPF32[outputPointer + frame];
    const probeStartedAt = traceSampleIndex >= 0 ? monotonicNow() : 0;
    if (traceSampleIndex >= 0 && trace) trace.stages.outputCopy[traceSampleIndex] = probeStartedAt - outputCopyStartedAt;
    this.processLatencyProbe(input, output);
    const channelCopyStartedAt = traceSampleIndex >= 0 ? monotonicNow() : 0;
    if (traceSampleIndex >= 0 && trace) trace.stages.latencyProbe[traceSampleIndex] = channelCopyStartedAt - probeStartedAt;
    for (let channel = 1; channel < (outputs[0]?.length ?? 0); channel += 1) outputs[0]?.[channel]?.set(output);
    const endedAt = monotonicNow();
    if (traceSampleIndex >= 0 && trace) trace.stages.channelCopy[traceSampleIndex] = endedAt - channelCopyStartedAt;
    this.recordCallbackTiming(startedAt, endedAt, frames, traceSampleIndex);
    return true;
  }
}
registerProcessor("apg-wasm-processor", ApgWasmProcessor);
