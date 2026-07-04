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

- Project mute and instance bypass remain runtime transport controls because they alter output samples.
- M7 static export now emits schedule, node, memory, process-symbol metadata, atom thunk tables, typed atom storage, initialized `atom_call_t` records, and init/refresh/process entrypoints. Measured CPU/stack budgets remain outside the core bundle.

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

## PC6 Progress

M7 static export now emits per-node atom process symbol names alongside schedule and node tables, giving firmware a deterministic link-plan view before full static call binding exists.

## PC7 Progress

M7 static export now emits a per-node `atom_thunk_fn` table that points at exported registry thunks, so generated bundles carry schedule order and callable atom entry points.

## PC8 Progress

M7 static export now emits section-placed `atom_call_t` records for each generated node with fixed process metadata.

## PC9 Progress

M7 static export now emits typed static atom storage plus init/refresh functions that bind signal pointers, signal-array pointers, state buffers, scalar params, and mix-matrix config without runtime YAML or allocation.

## PC10 Progress

M7 static export now emits `apg_m7_project_process_block()` and CTest links/runs the generated bundle against real atom thunks.

## PC11 Progress

CTest now compiles the generated M7 bundle to an object and verifies section placement for signal buffers, params, typed atom storage, and atom calls.

## PC12 Progress

CTest now measures generated M7 host block time in the linked smoke and checks generated runner stack usage when GCC or Clang stack-usage output is available.

## PC13 Progress

CMake now exposes opt-in production gates for ARM/M7 generated-runner stack usage and board timing through `APG_M7_C_COMPILER` and `APG_M7_BOARD_TIMING_COMMAND`.

## PC14 Progress

CMake now exposes an opt-in ARM/M7 link gate through `APG_M7_LINKER_SCRIPT` so generated bundles can be linked with a target memory map when a board script exists.

## PC15 Progress

M7 static export now accepts explicit board block-frame and sample-rate options. Generated timing macros, runtime image sizing, and export JSON reflect the selected board contract.

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

## PD9 Progress

Runtime image now exposes the schedule view used by runtime processing. The process loop no longer traverses `runtime->plan` for schedule iteration.

## PD10 Progress

Runtime reset and process entry checks now operate from runtime-owned buffers, image schedule metadata, and per-node compiled metadata pointers.

## PD11 Progress

Runtime image now exposes signal and parameter name maps, and runtime signal/param lookup APIs use those image-owned maps instead of traversing the source compiled plan.

## PD12 Progress

Runtime no longer stores a source compiled-plan pointer. Measure snapshots report runtime/image-derived counts and transport state without exposing compiled-plan identity, and meter reads use image-derived audio port maps.

## PD13 Progress

Runtime tests now initialize through explicit runtime-image builds instead of the former `apg_v2_runtime_init(...)` compatibility wrapper.

## PD14 Progress

The runtime plan-initializer compatibility API and runtime-owned image arena state have been removed; callers must build runtime images explicitly before runtime init.

## PD15 Progress

The no-op project solo runtime state was removed; solo remains a future host/UI routing concern, not runtime DSP state.

## PD16 Progress

Runtime nodes now execute image-owned atom thunks and labels instead of borrowing compiled node metadata.

## PD17 Progress

Runtime scalar refresh and signal-array binding plans now copy the needed binding values into runtime-image metadata instead of borrowing compiled binding structs.

## PD18 Progress

Runtime-image descriptors no longer store the source compiled-plan pointer. The image copies schedule/name tables it needs, and M7 export reads generated runtime artifacts from the image descriptor.

## PD19 Progress

Runtime-image consumer types now live separately from the compiler-backed builder API, so runtime headers no longer pull compiler plan definitions.

## PD20 Progress

Per-node runtime atom-call storage is now declared in an internal header. Public runtime headers expose only an opaque node pointer, not atom registry call storage.

## PD21 Progress

Meter snapshot structs now live in `measure_v2.h`, keeping meter read data on the measure API instead of the runtime API.

## PD22 Progress

Runtime bypass transport entries are now internal runtime state. Public runtime headers expose the bypass control API without the transport entry layout.

## PD23 Progress

`apg_v2_runtime_t` is now an opaque public handle. Runtime layout lives in `runtime_v2_internal.h`; host structs keep runtime ownership behind pointers and expose diagnostics through host helper APIs.

## PE1 Progress

Runtime image node layouts now record aligned offsets into one atom storage pool. Runtime initialization allocates that contiguous pool once and points per-node out/in/config/state storage into it.

## PE2 Progress

Runtime image node layouts now record per-buffer state sample offsets. Runtime initialization allocates one contiguous state-buffer pool and points per-node state buffer tables into it.

## PE3 Progress

M7 static export now emits section names and section-placed RAM buffers for signal buffers, params, atom storage, and state buffers. CTest verifies those generated bundle sections stay present.

## Immediate Direction

New tests and host/tooling code should read meters, snapshots, and diagnostics through `measure_v2` or host helpers, not public runtime fields. M7 work should next connect a real STM32H7 BSP timing command and production board linker script.
