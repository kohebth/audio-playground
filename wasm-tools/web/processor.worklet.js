import createApgProcessorModule from './apg_processor.mjs';

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
    if (request.type === "setMute") {
      this.command(request.id, this.module._apg_wasm_processor_set_mute(this.processor, request.enabled ? 1 : 0));
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
  recordCallbackTiming(startedAt, frames) {
    const elapsedMs = Math.max(0, Date.now() - startedAt);
    this.maxCallbackMs = Math.max(this.maxCallbackMs, elapsedMs);
    if (elapsedMs > (frames / sampleRate) * 1000) this.callbackDeadlineMisses += 1;
  }
  process(inputs, outputs) {
    const output = outputs[0]?.[0];
    if (!output) return true;
    if (!this.module || !this.processor || !this.ready) {
      output.fill(0);
      return true;
    }
    const startedAt = Date.now();
    const frames = output.length;
    if (frames > this.module._apg_wasm_processor_frame_capacity(this.processor)) {
      this.underruns += 1;
      output.fill(0);
      this.recordCallbackTiming(startedAt, frames);
      return true;
    }
    const inputPointer = this.module._apg_wasm_processor_input_buffer(this.processor) >>> 2;
    const input = inputs[0]?.[0];
    if (input) this.module.HEAPF32.set(input, inputPointer);
    else this.module.HEAPF32.fill(0, inputPointer, inputPointer + frames);
    const status = this.module._apg_wasm_processor_process(this.processor, frames);
    if (status !== 0) {
      this.underruns += 1;
      output.fill(0);
      this.recordCallbackTiming(startedAt, frames);
      return true;
    }
    const outputPointer = this.module._apg_wasm_processor_output_buffer(this.processor) >>> 2;
    for (let frame = 0; frame < frames; frame += 1) output[frame] = this.module.HEAPF32[outputPointer + frame];
    this.processLatencyProbe(input, output);
    for (let channel = 1; channel < (outputs[0]?.length ?? 0); channel += 1) outputs[0]?.[channel]?.set(output);
    this.recordCallbackTiming(startedAt, frames);
    return true;
  }
}
registerProcessor("apg-wasm-processor", ApgWasmProcessor);
