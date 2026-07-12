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
    this.meterCountdown = 0;
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
      let status = this.module._apg_wasm_processor_stage_image(this.processor, pointer, bytes.byteLength);
      this.module._free(pointer);
      if (status === 0) status = this.module._apg_wasm_processor_commit_staged(this.processor, BigInt(request.revision));
      this.reply(
        status === 0 ? { id: request.id, ok: true, type: "staged", revision: request.revision } : { id: request.id, ok: false, type: "error", message: `Runtime stage failed with status ${status}` }
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
    this.module._apg_wasm_processor_destroy(this.processor);
    this.processor = 0;
    this.ready = false;
    this.reply({ id: request.id, ok: true, type: "disposed" });
  }
  process(inputs, outputs) {
    const output = outputs[0]?.[0];
    if (!output) return true;
    if (!this.module || !this.processor || !this.ready) {
      output.fill(0);
      return true;
    }
    const frames = output.length;
    if (frames > this.module._apg_wasm_processor_frame_capacity(this.processor)) {
      output.fill(0);
      return true;
    }
    const inputPointer = this.module._apg_wasm_processor_input_buffer(this.processor) >>> 2;
    const input = inputs[0]?.[0];
    if (input) this.module.HEAPF32.set(input, inputPointer);
    else this.module.HEAPF32.fill(0, inputPointer, inputPointer + frames);
    const status = this.module._apg_wasm_processor_process(this.processor, frames);
    if (status !== 0) {
      output.fill(0);
      return true;
    }
    const outputPointer = this.module._apg_wasm_processor_output_buffer(this.processor) >>> 2;
    output.set(this.module.HEAPF32.subarray(outputPointer, outputPointer + frames));
    for (let channel = 1; channel < (outputs[0]?.length ?? 0); channel += 1) outputs[0]?.[channel]?.set(output);
    this.meterCountdown -= 1;
    if (this.meterCountdown <= 0) {
      this.meterCountdown = Math.max(1, Math.round(sampleRate / frames / 30));
      const meter = this.module._apg_wasm_processor_output_meter(this.processor);
      if (meter) {
        const floatWord = meter >>> 2;
        this.reply({
          id: 0,
          ok: true,
          type: "meter",
          meter: {
            peak: this.module.HEAPF32[floatWord] ?? 0,
            rms: this.module.HEAPF32[floatWord + 1] ?? 0,
            frames: this.module.HEAPU32[floatWord + 2] ?? 0,
            valid: (this.module.HEAPU32[floatWord + 3] ?? 0) !== 0
          }
        });
      }
    }
    return true;
  }
}
registerProcessor("apg-wasm-processor", ApgWasmProcessor);
