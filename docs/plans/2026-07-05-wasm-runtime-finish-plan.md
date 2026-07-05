# WASM Runtime Finish Plan

## Goal

Replace scaffolded WASM export with real browser realtime execution through AudioWorklet.

## Current Status

Scaffolded export exists; real runtime integration remains.

Done:

- `wasm_realtime` export emits deterministic scaffold artifacts.
- Web preview has a runtime adapter boundary and deterministic fallback behavior.

## Remaining Implementation

- [ ] Define final WASM export artifact contract:
  - manifest JSON
  - WASM module
  - AudioWorklet processor JS
  - adapter JS
- [ ] Preserve current scaffold manifest fields where possible.
- [ ] Implement compile/start/stop lifecycle in the browser adapter.
- [ ] Wire param updates, bypass updates, meter polling, and error reporting to stable runtime names.
- [ ] Reject unsupported atoms/features for `wasm_realtime` with stable diagnostics.

## Tests

- Export rejects unsupported atoms.
- Export emits expected files.
- JS adapter exposes stable method names.
- Add browser smoke only after the real worklet path exists.

## Exit Criteria

- Browser preview can run a project through realtime WASM/AudioWorklet, not deterministic render JSON.
- WASM export is no longer only a scaffold.
- Web UI can load the exported manifest and runtime artifacts.
