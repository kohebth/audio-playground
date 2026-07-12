/// <reference lib="webworker" />

import { loadModule, type ControlModule } from './emscripten';
import type { ControlRequest, ControlResponse } from './protocol';
import type { WasmDiagnostic } from './types';

const encoder = new TextEncoder();
let module: ControlModule | null = null;
let control = 0;

function writeBytes(bytes: Uint8Array): number {
  if (!module) throw new Error('Control module is not initialized');
  const pointer = module._malloc(bytes.byteLength || 1);
  if (!pointer) throw new Error('Control module allocation failed');
  module.HEAPU8.set(bytes, pointer);
  return pointer;
}

function readDiagnostic(): WasmDiagnostic {
  if (!module || !control) throw new Error('Control module is not initialized');
  const pointer = module._apg_wasm_control_last_diagnostic(control);
  if (!pointer) throw new Error('Control diagnostic is unavailable');
  const word = pointer >>> 2;
  const low = module.HEAPU32[word] ?? 0;
  const high = module.HEAPU32[word + 1] ?? 0;
  const text = (offset: number) => {
    const value = module?.HEAPU32[word + offset];
    return value && module ? module.UTF8ToString(value) : '';
  };
  return {
    revision: Number((BigInt(high) << 32n) | BigInt(low)),
    status: module.HEAPU32[word + 2] ?? 0,
    phase: text(3),
    code: text(4),
    file: text(5),
    path: text(6),
    message: text(7),
  };
}

function post(response: ControlResponse, transfer: Transferable[] = []) {
  self.postMessage(response, transfer);
}

function fail(id: number, error: unknown) {
  const fallback: WasmDiagnostic = {
    revision: 0,
    status: -1,
    phase: 'worker',
    code: 'APG_WORKER_ERROR',
    file: '',
    path: '',
    message: error instanceof Error ? error.message : String(error),
  };
  post({ id, ok: false, type: 'error', diagnostic: fallback });
}

async function handle(request: ControlRequest) {
  try {
    if (request.type === 'init') {
      module = await loadModule<ControlModule>(request.moduleUrl);
      control = module._apg_wasm_control_create(0);
      if (!control) throw new Error('Could not create WASM control handle');
      post({ id: request.id, ok: true, type: 'initialized' });
      return;
    }
    if (!module || !control) throw new Error('Control worker is not initialized');
    if (request.type === 'replaceWorkspace') {
      const entry = encoder.encode(request.snapshot.entryProject);
      const entryPointer = writeBytes(entry);
      let status = module._apg_wasm_control_begin_workspace(
        control,
        BigInt(request.snapshot.revision),
        entryPointer,
        entry.byteLength,
      );
      module._free(entryPointer);
      for (const file of request.snapshot.files) {
        if (status !== 0) break;
        const path = encoder.encode(file.path);
        const content = encoder.encode(file.content);
        const pathPointer = writeBytes(path);
        const contentPointer = writeBytes(content);
        status = module._apg_wasm_control_put_file(
          control,
          file.role === 'project' ? 1 : 2,
          pathPointer,
          path.byteLength,
          contentPointer,
          content.byteLength,
        );
        module._free(contentPointer);
        module._free(pathPointer);
      }
      if (status === 0) status = module._apg_wasm_control_validate_workspace(control);
      const diagnostic = readDiagnostic();
      post({
        id: request.id,
        ok: true,
        type: 'validated',
        result: { ok: status === 0, revision: request.snapshot.revision, diagnostic },
      });
      return;
    }
    if (request.type === 'prepare') {
      const config = module._malloc(16);
      if (!config) throw new Error('Could not allocate audio configuration');
      const configWord = config >>> 2;
      const revision = BigInt(request.revision);
      module.HEAPU32[configWord] = Number(revision & 0xffffffffn);
      module.HEAPU32[configWord + 1] = Number(revision >> 32n);
      module.HEAPU32[configWord + 2] = request.audio.sampleRate;
      module.HEAPU32[configWord + 3] = request.audio.blockFrames;
      const status = module._apg_wasm_control_prepare_workspace(control, config);
      module._free(config);
      if (status !== 0) {
        post({ id: request.id, ok: false, type: 'error', diagnostic: readDiagnostic() });
        return;
      }
      const sizePointer = module._malloc(4);
      const imagePointer = module._apg_wasm_control_prepared_image(control, sizePointer);
      const imageSize = module.HEAPU32[sizePointer >>> 2] ?? 0;
      module._free(sizePointer);
      if (!imagePointer || !imageSize) throw new Error('Prepared image is empty');
      const image = module.HEAPU8.slice(imagePointer, imagePointer + imageSize).buffer;
      const names = (count: number, getter: (index: number) => number) =>
        Array.from({ length: count }, (_, index) => module?.UTF8ToString(getter(index)) ?? '');
      const runtime = {
        revision: request.revision,
        imageBytes: imageSize,
        params: names(module._apg_wasm_control_param_count(control), index =>
          module?._apg_wasm_control_param_name(control, index) ?? 0,
        ),
        bypassInstances: names(module._apg_wasm_control_bypass_count(control), index =>
          module?._apg_wasm_control_bypass_name(control, index) ?? 0,
        ),
      };
      post({ id: request.id, ok: true, type: 'prepared', runtime, image }, [image]);
      return;
    }
    module._apg_wasm_control_destroy(control);
    control = 0;
    post({ id: request.id, ok: true, type: 'disposed' });
  } catch (error) {
    fail(request.id, error);
  }
}

self.onmessage = event => void handle(event.data as ControlRequest);
