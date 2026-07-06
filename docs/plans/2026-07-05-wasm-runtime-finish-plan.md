# WASM Runtime Finish Plan

## Goal

Replace scaffolded WASM export with real browser realtime execution through AudioWorklet.

## Current Status

Scaffolded export exists; real runtime integration remains.

Done:

- `wasm_realtime` export emits deterministic scaffold artifacts.
- Web preview has a runtime adapter boundary and deterministic fallback behavior.
- Exported adapter now has a browser AudioWorklet compile/start/stop lifecycle around the generated processor.

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
- [ ] Generate/load a real `apg_project_wasm.wasm` DSP module and invoke registry/runtime processing from the AudioWorklet processor.

Blocked external input:

- A WASM C toolchain is not currently available in the local environment (`emcc`, `clang`, and `wasm-ld` are absent).
- Ubuntu `emscripten` is available through apt, but installing it requires sudo/password access that this session does not have.

## Tests

- Export rejects unsupported atoms.
- Export emits expected files.
- JS adapter exposes stable method names.
- Add browser smoke only after the real worklet path exists.

## Exit Criteria

- Browser preview can run a project through realtime WASM/AudioWorklet, not deterministic render JSON.
- WASM export is no longer only a scaffold.
- Web UI can load the exported manifest and runtime artifacts.
