export type ControlModule = {
  HEAPU8: Uint8Array;
  HEAPU32: Uint32Array;
  _malloc: (size: number) => number;
  _free: (pointer: number) => void;
  UTF8ToString: (pointer: number) => string;
  _apg_wasm_control_create: (arenaBytes: number) => number;
  _apg_wasm_control_destroy: (control: number) => void;
  _apg_wasm_control_begin_workspace: (
    control: number,
    revision: bigint,
    entry: number,
    entryLength: number,
  ) => number;
  _apg_wasm_control_put_file: (
    control: number,
    role: number,
    path: number,
    pathLength: number,
    content: number,
    contentLength: number,
  ) => number;
  _apg_wasm_control_validate_workspace: (control: number) => number;
  _apg_wasm_control_prepare_workspace: (control: number, config: number) => number;
  _apg_wasm_control_prepared_image: (control: number, outSize: number) => number;
  _apg_wasm_control_param_count: (control: number) => number;
  _apg_wasm_control_param_name: (control: number, index: number) => number;
  _apg_wasm_control_bypass_count: (control: number) => number;
  _apg_wasm_control_bypass_name: (control: number, index: number) => number;
  _apg_wasm_control_last_diagnostic: (control: number) => number;
};

export type ProcessorModule = {
  HEAPU8: Uint8Array;
  HEAPF32: Float32Array;
  HEAPU32: Uint32Array;
  _malloc: (size: number) => number;
  _free: (pointer: number) => void;
  _apg_wasm_processor_create: () => number;
  _apg_wasm_processor_destroy: (processor: number) => void;
  _apg_wasm_processor_stage_image: (processor: number, image: number, imageSize: number) => number;
  _apg_wasm_processor_commit_staged: (processor: number, revision: bigint) => number;
  _apg_wasm_processor_input_buffer: (processor: number) => number;
  _apg_wasm_processor_output_buffer: (processor: number) => number;
  _apg_wasm_processor_frame_capacity: (processor: number) => number;
  _apg_wasm_processor_active_revision: (processor: number) => bigint;
  _apg_wasm_processor_process: (processor: number, frames: number) => number;
  _apg_wasm_processor_set_param: (processor: number, index: number, value: number) => number;
  _apg_wasm_processor_set_bypass: (processor: number, index: number, enabled: number) => number;
  _apg_wasm_processor_set_mute: (processor: number, enabled: number) => number;
  _apg_wasm_processor_reset: (processor: number) => number;
  _apg_wasm_processor_output_meter: (processor: number) => number;
};

type ModuleFactory<T> = (options?: { locateFile?: (path: string) => string }) => Promise<T>;

export async function loadModule<T>(moduleUrl: string): Promise<T> {
  const imported = (await import(/* @vite-ignore */ moduleUrl)) as { default?: ModuleFactory<T> };
  if (typeof imported.default !== 'function') throw new Error(`Invalid Emscripten module: ${moduleUrl}`);
  return imported.default({ locateFile: path => new URL(path, moduleUrl).href });
}
