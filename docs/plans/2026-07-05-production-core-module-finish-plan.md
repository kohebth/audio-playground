# Production Core Module Finish Plan

This is the canonical route to finish APGCore v2 production hardening. Do not create new per-module plans unless this file is proven wrong by code inspection. When continuing work, implement the next unchecked task here, update this file, and commit the slice.

## Finish Order

1. Finish compiler atom-layout materialization.
2. Finish runtime-image ownership and export parity.
3. Finish runtime real-time constraints.
4. Finish measure and host API closure.
5. Run production gates and close docs.

## Metadata

Status: functionally complete.

Done:

- Atom catalog owns target profile knowledge and compatibility hints.
- Atom catalog owns typed atom contract fields and required/optional flags.
- Compiler and validators consume catalog metadata instead of local schema tables.

Remaining:

- [ ] Add no new metadata APIs unless compiler/runtime-image cannot finish without them.
- [ ] Keep atom call pointers and registry descriptors isolated from parser/validator.

Exit criteria:

- Metadata remains the only source for atom contract/profile facts.
- `test_atom_catalog` covers any new catalog surface.

## Parser

Status: complete.

Done:

- Parser accepts YAML strings/files and emits raw contract graphs.
- Parser tests prove semantic-invalid contracts can still parse.
- Parser rejects malformed YAML syntax only.

Remaining:

- [ ] Do not add semantic validation to parser.

Exit criteria:

- Parser remains syntax-only.
- Parser boundary tests stay green.

## Validator

Status: complete.

Done:

- Unit/project validators own schema, semantic graph checks, known profiles, metadata references, compatibility, ports, params, routes, and scenes.
- Unknown unit compatibility profiles are rejected.
- Atom binding field validation stays in compiler.

Remaining:

- [ ] Do not move compiler-owned atom binding field checks into validator.

Exit criteria:

- Validator output remains the same validated graph shape.
- Validator tests cover profile and semantic rejection paths.

## Compiler

Status: one remaining boundary slice.

Done:

- Project compiler expands resolved projects into a namespaced unit graph.
- Unit compiler resolves atom metadata, params, signals, binding keys, scalar literals, signal arrays, float matrices, signal producers, topological schedule, and instance I/O facts.
- Runtime-image bypass endpoint inference has moved to compiler output.

Remaining:

- [ ] Add compiled atom-layout facts to each compiled node: atom name, thunk, storage sizes, input/config/state fields, and field counts.
- [ ] Make runtime-image consume compiled atom-layout facts instead of dereferencing `compiled_node.atom`.
- [ ] Keep `compiled_node.atom` as temporary compatibility only; do not remove it in this slice.
- [ ] Add compile/runtime-image tests proving runtime-image still builds after `compiled_node.atom` is nulled on a normal compiled plan.

Exit criteria:

- Compiler output contains every graph and atom-layout fact runtime-image needs.
- Runtime-image no longer needs raw atom registry entries for normal compiled plans.
- `./build-and-test.sh` passes.

## Runtime Image

Status: mostly complete; finish after compiler atom-layout slice.

Done:

- Runtime image owns compact signal, param, schedule, scalar refresh, signal binding, state buffer, control target, bypass, mute, meter, audio-port, and node layout metadata.
- Runtime image no longer stores the source compiled-plan pointer.
- Runtime image consumers no longer include compiler headers.

Remaining:

- [ ] After compiler atom-layout materialization, remove runtime-image raw registry dependency from normal node layout construction.
- [ ] Keep runtime-image as registration/layout only; do not add DSP execution logic.
- [ ] Confirm M7 export uses the same runtime-image facts as desktop runtime.

Exit criteria:

- Runtime image can be built from compiled graph facts without redoing graph inference.
- Runtime image is deterministic and arena-owned.
- Runtime-image tests pass after compiled-plan mutation.

## Runtime

Status: mostly complete.

Done:

- Runtime initializes from runtime image.
- Runtime executes image-owned schedule and atom thunks.
- Runtime owns audio/control execution state, not parsing, validation, compilation, or layout planning.
- Public runtime header exposes an opaque handle.

Remaining:

- [ ] Audit runtime process path for remaining name lookup, metadata lookup, allocation, or parser/compiler dependency.
- [ ] Keep bypass and mute as runtime transport controls because they change output samples.
- [ ] Add one focused test if audit finds a real-time-path lookup or allocation bug.

Exit criteria:

- Audio callback path walks prebuilt arrays and calls atom thunks only.
- No runtime-time YAML, graph traversal, allocation, or metadata resolution.
- Runtime tests and M7 generated runner tests pass.

## Measure

Status: mostly complete.

Done:

- Measure owns snapshots, meters, diagnostics, and host/tooling reads.
- Runtime read compatibility wrappers for meters and last error were removed.

Remaining:

- [ ] Audit host/tooling code for direct runtime internals reads that should go through measure.
- [ ] Keep measure read-only with respect to DSP execution state, except copying snapshots out.

Exit criteria:

- Host/tooling reads runtime state through measure APIs.
- Measure does not own DSP execution or mutation.
- `test_measure_v2` covers the public read paths.

## Host

Status: usable but not production-closed.

Done:

- Host APIs orchestrate resolved project load, compile, runtime-image build, runtime init, and mono processing.
- CLI validate/render/export paths exist.
- M7 static export has deterministic generated artifacts and configurable board gates.

Remaining:

- [ ] Audit host APIs for parser/validator/compiler/runtime-image ownership boundaries.
- [ ] Keep host as orchestration only; do not move DSP logic into host.
- [ ] Finish production deployment gates: STM32H7 board timing command, linker script gate, measured CPU budget, and real WASM AudioWorklet integration.

Exit criteria:

- Host can load, validate, compile, image, run, measure, and export without reaching into runtime internals.
- STM32H7/M7 production readiness is based on measured board gates, not host-only smoke tests.
- WASM export has a real runtime path, not only scaffold output.

## Final Closure

- [ ] Implement remaining module tasks in this file in order.
- [ ] Run `./build-and-test.sh` after backend implementation slices.
- [ ] Run web build/lint only for web changes.
- [ ] Update `plan.md`, `task.md`, and `docs/APGCORE_BOUNDARY_AUDIT.md` as slices complete.
- [ ] Commit each completed slice.
- [ ] Stop module hardening when every module exit criterion above is satisfied.
