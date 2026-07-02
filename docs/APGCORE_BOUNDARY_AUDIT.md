# APGCore v2 Boundary Audit

`core-design.md` is the production target. This audit tracks where the current C API already matches that target and where the next hardening slices should focus.

## Current Alignment

- `metadata`: `atom_catalog` and `atom_register` remain isolated reference data and atom call metadata.
- `parser`: `apg_v2_parse_string(...)` and `apg_v2_parse_file(...)` return raw YAML contract graphs without semantic checks.
- `validator`: unit and project validators own schema, metadata-reference, binding-key, compatibility, and route checks.
- `compiler`: unit and project compilers expand contracts, bind params/signals, and emit topological plans.
- `runtime image`: `apg_v2_runtime_image_build(...)` precomputes signal, param, meter, control-target, node, schedule, and state-buffer layout counts.
- `measure`: `apg_v2_measure_*` exposes snapshots, meters, and diagnostics for host/tooling reads.

## Boundary Leaks To Close

- Runtime still stores meter buffers and updates meter snapshots after processing. Keep this as an implementation detail for now, but host/tooling callers should use `measure_v2`.
- `apg_v2_runtime_init(...)` remains a compatibility wrapper that builds a temporary runtime image. Production host/export paths should prefer `apg_v2_runtime_image_build(...)` plus `apg_v2_runtime_init_from_image(...)`.
- Runtime node initialization still binds atom storage from compiled bindings. PB3 should move more binding layout decisions into runtime-image descriptors before STM32H7 static export is considered ready.
- Project mute, solo, and instance bypass are runtime controls today. Future host/control design should decide whether these become precompiled control targets or remain runtime-owned transport state.

## PB2 Progress

Host/tooling-facing render, smoke, load-all, offline-chain, hall-render, and project-compile tests now read diagnostics, meters, and project transport state through `measure_v2`. The remaining `apg_v2_runtime_last_error(...)` uses are runtime-specific compatibility tests and wrapper definitions.

## PB3 Progress

Runtime image now records per-node atom storage sizes and state-buffer counts. Runtime initialization consumes those image layouts instead of recomputing storage sizes and buffer counts directly from atom metadata.

## PB4 Progress

M7 static export artifacts declare no runtime YAML parser and no dynamic allocation. CTest now checks generated source for static schedule/node tables and rejects allocation, YAML, loader, or runtime-init symbols.

## Immediate Direction

New tests and host/tooling code should read meters, snapshots, and diagnostics through `measure_v2`, not runtime compatibility getters. Runtime should continue shrinking toward execution over prebuilt image metadata and atom call pointers only.
