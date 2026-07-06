# WASM Runtime Finish Plan

## Goal

Replace scaffolded WASM export with real browser realtime execution through AudioWorklet.

## Current Status

Scaffolded export exists; real runtime integration remains.

Done:

- `wasm_realtime` export emits deterministic scaffold artifacts.
- Web preview has a runtime adapter boundary and deterministic fallback behavior.
- Exported adapter now has a browser AudioWorklet compile/start/stop lifecycle around the generated processor.
- Export emits a registry-derived static WASM C bundle and the AudioWorklet processor loads/calls its exported block
  functions when `apg_project_wasm.wasm` is present.

## Remaining Implementation

- [x] Define final WASM export artifact contract:
  - manifest JSON
  - WASM module
  - AudioWorklet processor JS
  - adapter JS
- [x] Preserve current scaffold manifest fields where possible.
- [x] Implement compile/start/stop lifecycle in the browser adapter.
- [x] Wire param updates, bypass updates, meter polling, and error reporting to stable runtime names.
- [x] Reject unsupported atoms/features for `wasm_realtime` with stable diagnostics.
- [x] Generate/load a real `apg_project_wasm.wasm` DSP module and invoke registry/runtime processing from the AudioWorklet processor.

Toolchain note:

- `wasm_realtime` always emits `apg_project_wasm.h` and `apg_project_wasm.c`.
- Set `APG_WASM_EMCC` to an Emscripten command to additionally emit `apg_project_wasm.wasm`; otherwise the manifest
  records `wasm_module_available:false`.
- This slice was verified with a non-root `/tmp` Emscripten extraction via `APG_WASM_EMCC` and Node WebAssembly
  instantiation.

## Tests

- Export rejects unsupported atoms.
- Export emits expected files.
- JS adapter exposes stable method names.
- Generated WASM source exposes block process, input pointer, output pointer, and param setter symbols.
- Real `.wasm` export instantiates under Node when `APG_WASM_EMCC` is configured.
- Browser smoke remains a separate web/UI verification gate.

## Exit Criteria

- Browser preview can run a project through realtime WASM/AudioWorklet, not deterministic render JSON.
- WASM export is no longer only a scaffold.
- Web UI can load the exported manifest and runtime artifacts.
