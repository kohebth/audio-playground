# Unit v2 Compiler Architecture

## Loader

`apg_unit_v2_load_file(...)` and `apg_unit_v2_load_string(...)` parse YAML into `apg_unit_v2_t`. The loader validates the structural schema: required top-level fields, param types and bounds, audio/control port rules, unique graph signals, unique node IDs, duplicate binding keys, compatibility flags, known atom names, and `${params.name}` references.

The loader preserves string values in the caller-provided arena. It does not allocate runtime buffers or resolve signal indexes.

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

## Runtime MVP

`apg_v2_runtime_init(...)` creates `apg_v2_runtime_t` from a compiled plan. It owns signal buffers, param default values, and per-node `atom_call_t` storage. `apg_v2_runtime_find_input_port_signal(...)` and `apg_v2_runtime_find_output_port_signal(...)` expose named public audio port buffers for generic processing. `apg_v2_runtime_process_mono_ports(...)` copies a named mono input port into its signal buffer, refreshes config scalars from params/literals, executes the compiled schedule through atom thunks, and copies a named mono output port back to the caller. `apg_v2_runtime_process_mono(...)` remains a first-audio-port compatibility wrapper.

Current limits: public I/O is still mono-buffer oriented, atom in/out fields are bound by compiled binding order, and state allocation currently covers basic `FIELD_BUFFER` descriptors with conservative capacity.
