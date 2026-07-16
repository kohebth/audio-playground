# DSP Type System

## Purpose

The DSP type system defines the public C call ABI between atom implementations, registry/thunk code, runtimes, hosts,
and generated target bundles. It does not own DSP algorithms, YAML parsing, scheduling, or storage allocation.

`inc/atom/dsp_types.h` is the stable compatibility umbrella. Existing consumers can keep including it; consumers that
need one family may include that family's header directly.

## Header Ownership

| Header | Responsibility |
|---|---|
| `dsp_primitives.h` | Public `Signal`, `Spectrum`, and `Buffer` pointer aliases |
| `dsp_enums.h` | Shared enum tags, preferred namespaced typedefs, and legacy typedef aliases |
| `dsp_ports.h` | Generic public port-shape compatibility types |
| `dsp_type_macros.h` | Shared I/O member profiles and atom type declaration machinery |
| `*_types.h` | One family's atom-specific output, input, params, and state layouts |
| `dsp_type_checks.h` | Canonical-list coverage, duplicate-count protection, and macro cleanup for the umbrella |
| `dsp_types.h` | Include-only umbrella in dependency order |
| `dsp_atoms.h` | Process, spectral, and stream declarations generated from `APG_ATOM_DEFINITIONS` |

Families are `amplitude`, `delay`, `detect`, `filter`, `frequency`, `generation`, `interpolation`, `mix`,
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

## Real-Time Rules

- Fixed-rate process functions receive a valid immutable `apg_process_context_t` containing `frames`, `sample_rate`,
  and `sample_position`. Null, zero-frame, or invalid-rate contexts are no-ops; processing never falls back to 512
  frames implicitly.
- Variable-rate SRC functions receive `apg_stream_context_t`, report `apg_stream_result_t`, and retain phase in atom
  state. They cannot be placed in a fixed-rate unit schedule.
- No type declaration authorizes allocation, file/network I/O, locking, or unbounded setup in the process path.
- State-buffer sizing and allocation happen in registry/runtime preparation. Atoms may mutate their state but may not
  replace or free runtime-owned buffers. The registry derives bounded capacities from compiled delay, FIR, detector,
  and spectral configuration where possible, binds those capacities into state, and includes the pool in export memory
  manifests. Processing with a null buffer or zero capacity is a no-op; atoms never infer a historical fixed maximum.
- Input/output pointers are valid for the declared frame range of the current call only.
- Parameter updates and pointer-lifetime changes must be coordinated outside active processing.

## ABI Policy

Public typedef names, field names, field order, field C types, function names, and function parameter types are
versioned API surfaces. The LP64 ABI snapshot records 286 public types, 405 fields, and 17 enum values. A permanent
link test resolves 69 primary process functions and three additional spectral variants. Process inputs and params are
read-only; output and state remain mutable.

The historical phase-1 layout exception is the 46 former GNU zero-member structures. They became distinct standard
C11 structures containing `uint8_t _reserved`, changing size 0 to size 1 while preserving alignment 1. That exact
transition is checked against `dsp_types_abi_phase1_lp64.csv`. The current exact snapshot additionally records the
intentional `uint32_t phase` state for streaming up/down sampling, context-only sample rate, explicit overlap-buffer
capacities, removal of unused interpolation storage, and the single-buffer frequency-shift state. The frozen baseline
and phase-1 snapshots remain historical evidence; only the versioned current snapshot advances for intentional ABI
changes.

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

1. Add one row to the correct `APG_<FAMILY>_DSP_TYPE_TABLE` in `inc/atom/types/<family>_types.h`.
2. Reuse an I/O profile only when member names and semantics match exactly; add a profile to `dsp_type_macros.h` when
   the layout is genuinely new.
3. Put process-call bindings in input/output, configuration/control values in params, and persistent mutable data in
   state.
4. Add matching field descriptors in the family `*_field_descriptors.c`; record buffer bounds where applicable.
5. Add the canonical `APG_ATOM_DEFINITIONS` row with exact descriptor counts, capabilities, maturity, and dispatch.
6. Add a compiler/catalog contract row only when v2 metadata exposes that binding contract.
7. Implement the required process entry point without process-time allocation.
8. Update focused behavior tests, regenerate ABI evidence only for intentional changes, and run `./build-and-test.sh`.
9. Update the frozen atom catalog and TypeScript-facing fixtures when public metadata changes.

Contributor checklist:

```text
[ ] Atom placed in correct category header
[ ] Input/output fields represent process-call buffer bindings
[ ] Params contain configuration/control values only
[ ] State contains persistent runtime state only
[ ] Pointer ownership and lifetime are explicit
[ ] No process-time allocation introduced
[ ] Header compiles standalone
[ ] ABI test updated intentionally
[ ] C and TypeScript catalogs remain synchronized
```

## Generation Roadmap

Production family headers remain authoritative. `schema/atoms/src.json` and
`tools/generate_dsp_type_family.pl` are a candidate-only proof: CTest generates twice in the build tree, checks
determinism, and requires the body to match `src_types.h` byte for byte.

Migrate one family at a time only after schema output also matches canonical rows, field descriptors, catalog data, and
ownership/capacity metadata. Do not overwrite handwritten headers or generate algorithm sources until those comparison
tests exist. See `docs/refactor/dsp-type-generation-design.md` for the migration gate.
