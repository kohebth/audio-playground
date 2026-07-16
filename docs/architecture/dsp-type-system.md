# DSP Type System

## Purpose

The DSP type system defines the public C call ABI between atom implementations, registry/thunk code, runtimes, hosts,
and generated target bundles. It does not own DSP algorithms, YAML parsing, scheduling, or storage allocation.

`inc/atom/dsp_types.h` is the stable compatibility umbrella. Existing consumers can keep including it; consumers that
need one family may include that family's header directly.

## Header Ownership

| Header | Responsibility |
|---|---|
| `runtime/buffer.h` | Capacity-bearing mutable and immutable views used at public buffer boundaries |
| `runtime/prepare.h` | Required preparation context containing maximum block size and sample rate |
| `dsp_primitives.h` | Public `Signal`, `Spectrum`, and `Buffer` compatibility names backed by `apg_buffer_t` |
| `dsp_enums.h` | Shared enum tags, preferred namespaced typedefs, and legacy typedef aliases |
| `dsp_ports.h` | Generic public port-shape compatibility types |
| `dsp_common_types.h` | Shared enums, port layouts, and buffer primitives for every family |
| `dsp_type_macros.h` | Generated shared I/O member profiles and atom type declaration machinery |
| `*_types.h` | Generated family-owned output, input, params, and state layouts |
| `dsp_type_checks.h` | Canonical-list coverage, duplicate-count protection, and macro cleanup for the umbrella |
| `dsp_types.h` | Include-only umbrella in dependency order |
| `atom_definitions.h` | Shared flag policy plus generated canonical registry rows |
| `dsp_atoms.h` | Runtime context includes plus generated process, spectral, and stream declarations |

Families are `amplitude`, `delay`, `detect`, `filter`, `frequency`, `generation`, `interpolation`, `math`, `mix`,
`modulation`, `nonlinear`, and `src`. Every header is guarded and compiles standalone.

## Type Roles

Each atom owns four distinct public types:

- `*_out_t`: destinations bound for the current process call. Pointer fields refer to caller/runtime-owned output
  buffers and must not be retained by the atom.
- `*_in_t`: sources bound for the current process call. Pointer fields are borrowed for that call and must not be
  freed, resized, or retained as persistent state.
- `*_params_t`: configuration and control values. The v2 runtime owns the structure storage and refreshes scalar
  fields outside the atom algorithm. Pointer members are borrowed references; the pointee must outlive every process
  call that can read it. Atoms never free parameter pointees. Sample rate is not an atom parameter; rate-dependent
  atoms read it exclusively from the process or stream context.
- `*_state_t`: persistent mutable state across calls. The v2 runtime owns the structure and any descriptor-backed
  state-buffer pool. Every state buffer pointer has an explicit `uint32_t buffer_len`. Direct callers must keep both
  valid until processing ends and must provide the real nonzero capacity.

Process input and parameter structures are read-only. State remains mutable. New schema entries must record `value`,
`borrowed`, `runtime_owned`, or `external` ownership explicitly.

Every catalog-exposed parameter also has generated control metadata. `realtime: true` means a prepared instance may
accept the value without rebuilding its storage or topology. `structural: true` means compilation/preflight owns the
change and `realtime` must be false. Defaults, bounds, units, logarithmic scale hints, smoothing hints, and enum
ordinals are generated unchanged into the C catalog, TypeScript catalog, and atom JSON Schema.

Public runtime and host buffer boundaries use `apg_const_buffer_t {data, length}` for inputs and
`apg_buffer_t {data, capacity}` for outputs. Signal-buffer accessors also return views. Capacity is validated before
audio import, schedule execution, or output mutation. Generated atom I/O layouts and the compiled dispatcher retain
raw pointers internally after registry preflight so no per-sample capacity branch is added to the real-time plan.

## Real-Time Rules

- Fixed-rate process functions receive a valid immutable `apg_process_context_t` containing `frames`, `sample_rate`,
  and `sample_position`. Null, zero-frame, or invalid-rate contexts are no-ops; processing never falls back to 512
  frames implicitly.
- Registry and host preparation require a valid immutable `apg_prepare_context_t` containing `maximum_frames` and
  `sample_rate`. Invalid or missing preparation fails before allocation; there is no implicit 48 kHz rate.
- Variable-rate SRC functions receive `apg_stream_context_t`, report `apg_stream_result_t`, and retain phase in atom
  state. They cannot be placed in a fixed-rate unit schedule.
- No type declaration authorizes allocation, file/network I/O, locking, or unbounded setup in the process path.
- State-buffer sizing and allocation happen in registry/runtime preparation. Atoms may mutate their state but may not
  replace or free runtime-owned buffers. The registry derives bounded capacities from compiled delay, FIR, detector,
  and spectral configuration where possible, binds those capacities into state, and includes the pool in export memory
  manifests. Processing with a null buffer or zero capacity is a no-op; atoms never infer a historical fixed maximum.
- Input/output view data is valid for the declared sample range of the current call only. Resolved internal atom
  pointers are valid for the prepared block capacity.
- Parameter updates and pointer-lifetime changes must be coordinated outside active processing.

## Canonical Primitives

Conceptually duplicate atoms share one internal algorithm kernel. The preferred names carry the public or advanced
surface; legacy names remain registered and loadable as internal compatibility entries so existing unit metadata does
not break.

| Preferred atom | Visibility | Internal compatibility entries | Contract |
|---|---|---|---|
| `generation_oscillator` | Public | `generation_lfo` | One oscillator kernel; the compatibility entry omits the optional frequency signal |
| `math_difference` | Advanced | `detect_slope`, `filter_differentiate` | `y[n] = x[n] - x[n-1]` |
| `math_integrate` | Advanced | `amplitude_accumulate`, `filter_integrate` | Configurable leakage; compatibility presets are `1.0` and `0.999` |
| `mix_crossfade` | Public | `mix_wet_dry` | Linear or equal-power curve; compatibility entry uses the linear curve |

The default editor palette therefore exposes only the preferred public atoms. Advanced mode exposes the canonical
math primitives; internal compatibility entries can be loaded but cannot be newly added from the palette.

## ABI Policy

Public typedef names, field names, field order, field C types, function names, and function parameter types are
versioned API surfaces. The LP64 ABI snapshot records 296 public types, 424 fields, and 17 enum values. A permanent
link test resolves 71 primary process functions and three additional spectral variants. Process inputs and params are
read-only; output and state remain mutable.

The historical phase-1 layout exception is the 46 former GNU zero-member structures. They became distinct standard
C11 structures containing `uint8_t _reserved`, changing size 0 to size 1 while preserving alignment 1. That exact
transition is checked against `dsp_types_abi_phase1_lp64.csv`. The current exact snapshot additionally records the
intentional `uint32_t phase` state for streaming up/down sampling, context-only sample rate, explicit overlap-buffer
capacities, removal of unused interpolation storage, the single-buffer frequency-shift state, the two canonical math
atoms, the crossfade curve selector, and the replacement of raw `Signal`/`Spectrum`/`Buffer` aliases with
capacity-bearing views. The frozen baseline and phase-1 snapshots remain historical evidence; only the versioned
current snapshot advances for intentional ABI changes.

Pointer-containing sizes are platform-dependent. Do not copy LP64 size assertions to 32-bit targets. Atom structures
are not raw persistence formats; YAML, registry plans, WASM images, and M7 bundles use explicit metadata and planned
layouts instead.

## Enum Policy

Use `apg_waveform_type_t`, `apg_normalize_mode_t`, `apg_interpolation_type_t`, and `apg_window_type_t` in new code.
`WaveformType`, `NormalizeMode`, `InterpolationType`, and `WindowType` remain source-compatible aliases. Legacy
enumerator names and all numeric values are preserved and explicitly assigned.

Atom parameter fields remain `int`, and persisted metadata carries numeric scalar values rather than compiler enum
objects. C enum storage width is not part of a file or wire contract.

## Empty Types

An atom role with no semantic fields still has a distinct structure:

```c
typedef struct {
    uint8_t _reserved;
} example_atom_state_t;
```

`_reserved` is not a config/state descriptor, UI control, or logical state field. Do not replace these types with GNU
empty structs, zero-length arrays, shared aliases, or `void`. A future nullable absent-storage design would require a
separate runtime ABI version.

## Adding An Atom

1. Add one atom object to `schema/atoms/atoms.json` in canonical registry order.
2. Reuse an I/O profile only when member names and semantics match exactly; add a schema profile when the layout is
   genuinely new.
3. Put process-call bindings in input/output, configuration/control values in params, and persistent mutable data in
   state. Record ownership for every field and capacity metadata for every runtime-owned state buffer.
4. Select the capability, maturity, dispatch, descriptor, and catalog metadata in the same atom object.
5. Assign exactly one visibility and provide metadata for every exposed config field. Public atoms appear in the
   default editor; advanced atoms are opt-in; internal atoms remain loadable but cannot be created from the palette.
6. Regenerate through `cmake --build build --target generate_atom_artifacts`; never edit generated family headers or
   descriptor sources directly.
7. Implement the required handwritten process entry point without process-time allocation.
8. Update focused behavior tests, regenerate ABI evidence only for intentional changes, and run `./build-and-test.sh`.
9. Update the frozen atom catalog when public metadata changes, then build and lint `web-tools`.

Contributor checklist:

```text
[ ] Atom placed in the correct schema family and registry order
[ ] Input/output fields represent process-call buffer bindings
[ ] Params contain configuration/control values only
[ ] State contains persistent runtime state only
[ ] Pointer ownership and lifetime are explicit
[ ] Visibility and complete parameter metadata are present
[ ] Runtime-owned state buffers have explicit capacity and buffer_len metadata
[ ] No process-time allocation introduced
[ ] Generated artifacts are current and family headers compile standalone
[ ] ABI test updated intentionally
[ ] C and TypeScript catalogs remain synchronized
```

## Generation Ownership

`schema/atoms/atoms.json` is authoritative for all family type tables, canonical rows, public declarations, field
descriptors, backend catalog contracts, the TypeScript catalog, and atom-binding JSON Schema. Generated outputs are
checked in and carry a do-not-edit banner.

`test_atom_artifact_generation` proves deterministic output, checks the repository for drift, and verifies that a
deliberately changed output is rejected. Use `generate_atom_artifacts` to rewrite outputs and `check_atom_artifacts` in
local or CI verification. See `docs/refactor/dsp-type-generation-design.md` for the complete boundary and commands.
