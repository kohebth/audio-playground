# APG v1 Deprecation Audit

APG v1 is legacy. Runtime, control, and unit-loader code has been removed from the production build.

## Still Active

- Legacy `units/*.unit.yaml` files were removed after migration to `units-v2/*.unit.v2.yaml` fixtures.

## Current Deprecation Fence

- v1 runtime, control, and v1 unit-loader public APIs have been removed.
- `*.v1` in contract JSON schema strings are protocol version labels for interoperability snapshots, not reintroduced legacy runtime behavior.
- No default CTest target is currently labelled `legacy_v1`.
- `test_runtime_process_frames` has been migrated to APGCore v2.
- `test_ctrl_transition` has been migrated to APGCore v2 control-port smoothing.
- `test_unit_load_all` has been migrated to APGCore v2 fixture load/compile/runtime smoke coverage.
- `test_offline_chain` has been migrated to an APGCore v2 project runtime offline-chain regression.
- `test_hall_reverb` has been migrated to an in-memory APGCore v2 pedalboard offline-render regression.
- `src/test_runtime.c` has been migrated to an APGCore v2 host smoke utility.
- The old PipeWire `src/live.c` app and `fast_chunk` helper have been removed.
- Default CMake source groups no longer compile v1 runtime/control sources or the v1 YAML unit loader into v2 targets.
- v1 runtime/control implementation, headers, and unit-loader implementation have been removed from source.
- The old `inc/yaml/unit.h` v1 unit shape has been removed; v2 bindings use `apg_v2_value_t`.
- Fixed-size `src/unit` adapter helpers and their direct adapter test have been removed.
- Clean tracked v1 fixture files have been removed; only pre-existing modified or untracked `units/` files remain.
- v2 parser, validator, compiler, runtime image, runtime, and measure modules are the production path.

## Removal Blockers

- v1 `units/*.unit.yaml` fixtures have been deleted; legacy references were reviewed and no longer referenced by tests or runtime workflows.
- Keep `src/yaml/arena.c`, `lexer.c`, `parser.c`, and shared node/error utilities unless a replacement parser is chosen.
