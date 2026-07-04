# Unit v2 Compiler Architecture

`core-design.md` is the current production module-boundary target: metadata, parser, validator, compiler, runtime image, runtime, measure, and host.

## Production Module Flow

```mermaid
flowchart LR
    Source["YAML Source"] --> Parser["parser"]
    Parser --> Validator["validator"]
    Metadata["metadata"] --> Validator
    Validator --> Compiler["compiler"]
    Metadata --> Compiler
    Compiler --> RuntimeImage["runtime image"]
    RuntimeImage --> Runtime["runtime"]
    RuntimeImage --> Measure["measure"]
    Runtime --> Measure
    Metadata --> Runtime
    RuntimeImage --> Host["host"]
    Measure --> Host
    Host --> Runtime
```

## Parser and Validator

`apg_v2_parse_file(...)` and `apg_v2_parse_string(...)` parse YAML into a raw arena-owned `uc_node` contract graph without semantic validation. Unit and project validators then fill `apg_unit_v2_t` or `apg_project_v2_t` and validate schema rules, atom metadata references, graph names, binding keys, compatibility flags, and `${params.name}` references.

The public loaders remain thin compatibility entry points that run parser then validator. They preserve string values in the caller-provided arena and do not allocate runtime buffers or resolve signal indexes.

For production DSP execution, parser/validator/compiler/image stages are intentionally out of the real-time path: runtime initialization and `apg_v2_runtime_process_*` operate only on prebuilt metadata and immutable schedules.

## Compiler Plan

`apg_v2_compile_unit(...)` lowers `apg_unit_v2_t` into `apg_v2_compiled_unit_t`:

- resolves atom registry entries per node;
- converts signal bindings into signal indexes;
- converts config bindings into param indexes or literal values;
- validates required atom binding keys for the MVP atom set;
- records `signal_producers[signal_index]` for runtime lookup;
- emits a schedule of node indexes.

Compile errors include node IDs, binding sections, and binding keys where available so fixture failures point at the source graph entry.

## Scheduling

The compiler treats public audio inputs as initially available, then repeatedly schedules nodes whose signal inputs are available. Forward references are allowed when a later node produces the needed signal. Unresolved dependencies and direct cycles fail compilation.

This scheduling step guarantees a fixed, validated execution order before any audio callback or render call starts.

## Runtime MVP

`apg_v2_runtime_image_build(...)` creates an arena-owned runtime image descriptor from a compiled plan. It precomputes signal, param defaults, smoothing frames, meter, public audio port maps, bypass/project-mute metadata, per-node atom storage offsets, schedule view, state-buffer count/capacity/offsets, signal-array pointer-pool sizing, scalar refresh plans, and control-target layout metadata without allocating audio/runtime buffers. Scalar literal text is parsed by the compiler before runtime, and scalar refresh field/key validation stops at image build.

`apg_v2_runtime_init_from_image(...)` creates `apg_v2_runtime_t` from that descriptor. The runtime owns signal buffers, param/default tables, image-derived lookup/control targets, and per-node `atom_call_t` storage, but not the source compiled-plan pointer or runtime-image arena. Runtime nodes execute atom thunks and labels copied from the runtime image. `apg_v2_runtime_find_input_port_signal(...)` and `apg_v2_runtime_find_output_port_signal(...)` expose the first channel of named public audio ports, while explicit channel APIs expose mapped channel buffers. `apg_v2_runtime_set_param(...)` and `apg_v2_runtime_set_control_port(...)` use runtime-image metadata. `apg_v2_runtime_reset(...)` clears signal buffers, restores image-derived param defaults, and resets state storage while preserving internal buffer pointers. Processing APIs copy named external buffers, refresh scalar bindings, execute the runtime-image schedule through atom thunks, and copy outputs back to the caller. Meter snapshots are produced by `apg_v2_measure_*` from runtime signal buffers.

`apg_v2_measure_*` APIs expose host/tooling reads for runtime snapshots, meters, and diagnostics. There are no remaining runtime read wrappers for these paths; callers use `measure_v2` directly. See `docs/APGCORE_BOUNDARY_AUDIT.md` for the current production boundary audit.

## Memory Ownership

The loader and compiler write all parsed unit data, binding arrays, schedules, and producer maps into the caller-provided `uc_arena`. The compiled plan borrows the loaded unit pointer, so the arena must outlive both `apg_unit_v2_t` and `apg_v2_compiled_unit_t`. Treat loaded units and compiled plans as immutable once a runtime has been initialized from them.

The runtime image borrows the compiled plan and stores arena-owned metadata/default/layout tables. The runtime borrows image-derived metadata and owns only its mutable allocations: signal pool, signal pointer table, parameter/default/target values, bypass state flags, one contiguous atom storage pool, and one contiguous state-buffer pool. Runtime lookup APIs return pointers into owned buffers; callers must not free them and must stop using them after `apg_v2_runtime_destroy(...)`. Destroying a runtime frees only runtime-owned memory and does not free the arena, runtime image, or compiled plan.

Current limits: control ports update params only, atom in/out fields are bound by compiled binding order, and state allocation currently covers `FIELD_BUFFER` descriptors with atom-declared capacities. The v1 runtime/unit/YAML paths are legacy; see `docs/V1_DEPRECATION_AUDIT.md` before removing anything.

Schema names in CLI fixture outputs such as `apg.unit.inspect.v1` and `apg.project.render.v1` are protocol identifiers only; they do not indicate runtime re-use of a legacy v1 execution path.

For STM32H7/M7 deployment boundaries, see `docs/STM32H7_M7_BOARD_INTEGRATION.md`.
