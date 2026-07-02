# APG v1 Deprecation Audit

APG v1 is legacy. Runtime, control, and unit-loader code has been removed from the production build.

## Still Active

- `src/unit/*.c` and `inc/unit/*.h`: fixed-size adapter tests still cover these direct atom-composition helpers.
- `units/*.unit.yaml`: v1 fixtures retained only for local legacy experiments.

## Current Deprecation Fence

- v1 runtime, control, and v1 unit-loader public APIs have been removed.
- No default CTest target is currently labelled `legacy_v1`.
- `test_runtime_process_frames` has been migrated to APGCore v2.
- `test_ctrl_transition` has been migrated to APGCore v2 control-port smoothing.
- `test_unit_load_all` has been migrated to APGCore v2 fixture load/compile/runtime smoke coverage.
- `test_offline_chain` has been migrated to an APGCore v2 project runtime offline-chain regression.
- `test_hall_reverb` has been migrated to an in-memory APGCore v2 pedalboard offline-render regression.
- `src/test_runtime.c` has been migrated to an APGCore v2 host smoke utility and now builds without PipeWire.
- `src/live.c` has been migrated to APGCore v2 host-unit loading and runtime processing.
- Default CMake source groups no longer compile v1 runtime/control sources or the v1 YAML unit loader into v2 targets.
- v1 runtime/control implementation, headers, and unit-loader implementation have been removed from source.
- v2 parser, validator, compiler, runtime image, runtime, and measure modules are the production path.

## Removal Blockers

- Replace any needed `units/*.unit.yaml` fixtures with `units-v2/*.unit.v2.yaml` or project-v2 fixtures.
- Keep `src/yaml/arena.c`, `lexer.c`, `parser.c`, and shared node/error utilities unless a replacement parser is chosen.
