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

- Runtime no longer stores meter snapshots or updates meter buffers. Metering is computed on demand in `measure_v2` from latest runtime signal buffers.
- `apg_v2_runtime_init(...)` remains a compatibility wrapper that builds a temporary runtime image. Production host/export paths should prefer `apg_v2_runtime_image_build(...)` plus `apg_v2_runtime_init_from_image(...)`.
- Runtime node initialization consumes precomputed signal/config binding plans from runtime-image. Project mute/solo and instance bypass remain runtime transport controls for now.
- Host/control design for mute/solo/bypass can move further toward precompiled control targets if a strict non-allocating runtime control interface is required.

## PB2 Progress

Host/tooling-facing render, smoke, load-all, offline-chain, hall-render, project-compile, and runtime tests now read diagnostics, meters, and project transport state through `measure_v2`. Runtime compatibility read wrappers for measure and diagnostics have been removed.

## PB3 Progress

Runtime image now records per-node atom storage sizes and state-buffer counts. Runtime initialization now consumes precomputed signal/config binding plans and no longer resolves signal/config pointers from compiled binding descriptors.

## PB4 Progress

M7 static export artifacts declare no runtime YAML parser and no dynamic allocation. CTest now checks generated source for static schedule/node tables and rejects allocation, YAML, loader, or runtime-init symbols.

## PC1 Progress

M7 static export now emits a deterministic memory manifest in JSON and generated header macros, including block frames, signal buffer bytes, param bytes, schedule bytes, atom storage bytes, state buffer bytes, and total static RAM bytes.

## PC2 Progress

M7 static export accepts `--max-static-ram <bytes>` and rejects compatible bundles whose computed static RAM manifest exceeds the provided board budget.

## PC3 Progress

M7 static export tests now run an ARM/M7 freestanding syntax check for generated bundles when `APG_M7_C_COMPILER` or `arm-none-eabi-gcc` is available. Without a configured toolchain, CTest records an explicit skipped-gate message.

## PC4 Progress

`docs/STM32H7_M7_BOARD_INTEGRATION.md` defines the fixed-block audio callback, DMA ownership, cache maintenance, memory placement, control, and measure contract expected from a future STM32H7 board support package.

## PC5 Progress

M7 static export now emits static atom-call workload metadata in JSON and generated header macros. This is a schedule-derived budget input, not measured CPU timing.

## PD1 Progress

Runtime image node layouts now record per-node state-buffer sample counts. Runtime initialization allocates state buffers from image metadata instead of using atom descriptor capacities directly.

## PD2 Progress

Runtime image node layouts now record signal-array pointer pool sizes. Runtime initialization uses one pre-sized per-node pointer pool for signal-array bindings instead of allocating one auxiliary block per binding.

## PD3 Progress

Runtime image node layouts now record scalar config and scalar input refresh plans. Runtime processing walks those compact entries instead of scanning binding keys and atom field metadata per node.

## PD4 Progress

Compiler scalar bindings now store parsed numeric literals. Runtime scalar refresh reads the compiled float value instead of parsing binding text.

## PD5 Progress

Runtime image now precomputes parameter smoothing frame counts from unit metadata and sample rate. Runtime control updates reuse those counts without parsing text.

## PD6 Progress

Control-port updates now apply through runtime-image parameter indexes instead of doing a second parameter-name lookup.

## PD7 Progress

Default mono processing now uses the first image-owned audio-port map entries directly instead of converting those entries back through port-name lookup.

## PD8 Progress

Scalar refresh binding-key validation now happens during runtime-image build. Runtime processing applies image-validated offsets and field types without per-block key comparisons.

## PE1 Progress

Runtime image node layouts now record aligned offsets into one atom storage pool. Runtime initialization allocates that contiguous pool once and points per-node out/in/config/state storage into it.

## PE2 Progress

Runtime image node layouts now record per-buffer state sample offsets. Runtime initialization allocates one contiguous state-buffer pool and points per-node state buffer tables into it.

## PE3 Progress

M7 static export now emits section names and section-placed RAM buffers for signal buffers, params, atom storage, and state buffers. CTest verifies those generated bundle sections stay present.

## Immediate Direction

New tests and host/tooling code should read meters, snapshots, and diagnostics through `measure_v2`, not runtime compatibility getters. Runtime should continue shrinking toward execution over prebuilt image metadata and atom call pointers only.
