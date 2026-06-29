# APGCore v2 Implementation Plan

This plan tracks completed work and the remaining phase-by-phase path for the APGCore v2 unit format, compiler, and runtime.

## Current Snapshot

- Phase 0 variable-frame atom migration is complete.
- Phase 1 explicit-frame runtime/control/unit adapter work is complete.
- The v2 loader, schema fixtures, compiler contracts, topological scheduler, and runtime MVP are implemented.
- The current verification workflow is `./build-and-test.sh`, run once per completed implementation slice before committing.

## Completed Work

### Foundation and Adapters

- [x] Added `apg_process_info_t` and routed atom execution through explicit process metadata.
- [x] Migrated atom families away from direct `CHUNK_LENGTH` assumptions, including delay, filter, modulation, detector, mix, nonlinear, interpolation, SRC, and spectral atoms.
- [x] Added `output_frames` capacity metadata for atoms that can produce variable output lengths.
- [x] Split the large atom test driver into focused test translation units with a shared harness.
- [x] Added explicit-frame runtime, control, live, chorus, and sustainer adapter entry points while preserving compatibility wrappers.

### V2 Schema and Loader

- [x] Documented the initial `unit.v2.yaml` schema in `docs/schemas/unit-v2.md`.
- [x] Added small deterministic fixtures under `units-v2/` for gain, mix, and clip behavior.
- [x] Implemented loader validation for required fields, params, ports, graph signals, node IDs, binding duplicates, compatibility flags, known atoms, and param references.
- [x] Added tests for invalid schemas, duplicate names, unsupported fields, and fixture loading.

### V2 Compiler

- [x] Added `apg_v2_compile_unit(...)` to resolve atoms, signal indexes, config bindings, and schedule entries.
- [x] Added binding contract validation for MVP atoms: `generation_dc`, amplitude arithmetic, clipping, and `mix_wet_dry`.
- [x] Added dataflow validation for unproduced signals and public output signals.
- [x] Added topological scheduling so forward references compile when dependencies are valid.
- [x] Stored signal producer indexes in compiled plans for runtime lookup.
- [x] Improved compile errors with node IDs, binding section names, and binding keys.

### V2 Runtime

- [x] Added `apg_v2_runtime_t` with owned signal buffers, params, atom calls, and state buffer cleanup.
- [x] Added runtime init, signal lookup, param update, generic schedule execution, mono processing, and last-error reporting.
- [x] Executed `simple_gain`, `simple_clip`, and `simple_mix` fixtures through runtime tests.
- [x] Added state `FIELD_BUFFER` allocation and partial-init cleanup coverage.

### Tooling and Documentation

- [x] Added `./build-and-test.sh` as the standard configure/build/test wrapper.
- [x] Added `docs/UNIT_V2_ARCHITECTURE.md` for loader, compiler, scheduling, and runtime notes.
- [x] Refreshed `AGENTS.md` with current repository context, `fsmcp` editing rules, `clang-format` approval, and `git commit` approval.

## Remaining Phases

### Phase H: Atom Contract Expansion

- [x] H1: Add compiler binding metadata and tests for `delay_line`.
- [x] H2a: Add compiler binding metadata and tests for runtime-compatible delay atoms: `delay_unit` and `delay_fractional`.
- [x] H2b: Add a binding model for non-signal atom inputs before validating delay tap atoms.
- [x] H3a: Add compiler binding metadata and tests for runtime-compatible filters: `filter_biquad`, `filter_allpass`, `filter_comb_ff`, and `filter_dc_block`.
- [x] H3b: Add optional binding support before validating `filter_comb_fb`.
- [x] H4a: Add compiler binding metadata and tests for modulation atoms and scalar/stereo mix atoms.
- [x] H4b: Add array/matrix binding support before validating `mix_matrix`.
- [x] H5: Refresh schema documentation with the newly supported atom contracts; deferred contract gaps are tracked in `problem.md`.

### Phase I: Runtime I/O Model

- [x] I1: Define a named public port binding API instead of relying only on first mono input/output helpers.
- [x] I2a: Reject multi-channel public ports in mono processing APIs with useful runtime errors.
- [x] I2b: Add true multi-channel audio port buffer binding after defining channel-to-signal mapping.
- [x] I3a: Add control input ingestion for same-named params.
- [x] I3b: Define explicit control-to-param routing beyond same-named param updates.
- [x] I4: Add tests for multi-input, multi-output, and rejected mismatched buffer layouts.

### Phase J: Runtime State and Config Maturity

- [x] J1: Size state buffers from atom descriptor metadata instead of a single conservative capacity.
- [x] J2: Support additional descriptor field types such as `FIELD_FLOAT_PTR` and `FIELD_FLOAT_PP` where needed.
- [x] J3: Add runtime reset support for stateful nodes.
- [x] J4: Add deterministic runtime fixtures for delay, filter, and modulation state behavior.

### Phase K: Fixture Library Expansion

- [x] K1: Add `units-v2/` fixtures for delay, filter, modulation, stereo, control-port, and matrix-mix examples.
- [x] K2: Compile all v2 fixtures in one test path and runtime-execute all runtime-capable fixtures.
- [x] K3: Keep fixture audio deterministic and small; avoid committing generated audio unless it is an intentional test fixture.

### Phase L: API and Error Polish

- [x] L1: Add public API documentation comments for v2 loader, compiler, and runtime functions.
- [x] L2: Standardize runtime error text to include node ID, atom name, and failing binding where possible.
- [x] L3: Review memory ownership rules for arenas, compiled plans, and runtime buffers.

### Phase M: Integration and Migration

- [x] M1: Bridge v2 runtime execution into the offline or live host path behind an explicit opt-in.
- [x] M2: Draft a migration path from selected v1 units to v2 fixtures.
- [x] M3: Add benchmarks or regression checks for representative v1 and v2 chains.

### Phase N: Tooling and CI

- [x] N1: Add focused build targets or labels for v2 loader/compiler/runtime tests if useful.
- [x] N2: Add sanitizer or debug verification jobs when the local workflow is stable.
- [ ] N3: Keep `AGENTS.md`, `task.md`, and this plan aligned when workflow rules change.

## Execution Order

1. Complete Phase H atom contracts in small slices, starting with `delay_line`.
2. Expand runtime I/O only after enough atom contracts exist to build realistic v2 graphs.
3. Improve runtime state/config support before adding large fixture families.
4. Integrate v2 into host paths only after compiler/runtime behavior is covered by focused tests.

## Per-Phase Workflow

1. Pick one checkbox or a tight group of related checkboxes.
2. Implement the smallest code and test change that proves the behavior.
3. Format touched C/H files with `clang-format` when source layout changes.
4. Run `./build-and-test.sh` exactly once for the completed slice.
5. Commit with `git commit -m "<which tasks are done>"`.
