# APG v1 Deprecation Audit

APG v1 is legacy and should be removed only after the remaining default-build dependencies are migrated or deleted.

## Still Active

- `src/rte/runtime.c` and `inc/rte/runtime.h`: used by `test_unit_load_all`, `test_offline_chain`, `test_hall_reverb`, `src/live.c`, and `src/test_runtime.c`.
- `src/ctrl/ctrls.c` and `inc/ctrl/ctrls.h`: used by `test_ctrl_transition` and optional live control paths.
- `src/unit/*.c` and `inc/unit/*.h`: fixed-size v1 adapters still covered by adapter tests.
- `src/yaml/loader.c` and `inc/yaml/loader.h`: v1 unit loader; the lower-level YAML lexer/parser is shared by v2 parser wrappers and must not be removed.
- `units/*.unit.yaml`: v1 fixtures used by the legacy tests and optional live examples.

## Current Deprecation Fence

- v1 runtime, control, and v1 unit-loader public APIs have opt-in deprecated attributes behind `APG_ENABLE_V1_DEPRECATED_WARNINGS`.
- v1-dependent CTest targets are labelled `legacy_v1`.
- `test_runtime_process_frames` has been migrated to APGCore v2.
- v2 parser, validator, compiler, runtime image, runtime, and measure modules are the production path.

## Removal Blockers

- Migrate or retire the legacy tests listed above.
- Decide whether `src/live.c` should move to APGCore v2 or be removed.
- Replace any needed `units/*.unit.yaml` fixtures with `units-v2/*.unit.v2.yaml` or project-v2 fixtures.
- Keep `src/yaml/arena.c`, `lexer.c`, `parser.c`, and shared node/error utilities unless a replacement parser is chosen.
