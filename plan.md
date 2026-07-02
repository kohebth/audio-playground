# APGCore v2 Implementation Plan

This plan tracks completed work and the remaining phase-by-phase path for the APGCore v2 unit format, compiler, and runtime.

## Current Snapshot

- Phase 0 variable-frame atom migration is complete.
- Phase 1 explicit-frame runtime/control/unit adapter work is complete.
- The v2 loader, schema fixtures, compiler contracts, topological scheduler, runtime MVP, host bridge, control routing, atom catalog export, project validation, project compilation, JSON contracts, runtime controls, product fixtures, web handoff package, and MVP phases AC-AJ are implemented.
- The Audio Playground v2 MVP now supports project-level visual build/edit/save/validate/preview/export workflows from frozen backend contracts while `apgcore` remains portable C11 and ARM/M7-aware.
- Production readiness is not achieved yet. STM32H7/M7 deployment still needs target export validation, cross-compilation, board audio integration, and measured memory/CPU budgets.
- Final verification passed with `npm run test`, `npm run build`, and `npm run lint` in `web-tools/unit-editor`, plus `./build-and-test.sh` across 20 CTest targets.
- Remaining work is production hardening, primarily STM32H7/M7 readiness and real WASM AudioWorklet preview/export beyond the deterministic preview and blocked export skeleton.
- Current production architecture target: isolate `metadata`, `parser`, `validator`, `compiler`, runtime image, `runtime`, `measure`, and `host` as described in `core-design.md`, so the real-time path only executes a compact prebuilt schedule over registered memory.

## Production Implementation Lifecycle

1. Implement as much as possible module-by-module.
2. Record pending tests in this plan and `task.md` before broad implementation.
3. After each completed implementation module, add a module note of 40 words or fewer.
4. Hardware-facing work must pass generated artifact checks, cross-compile checks, fixed memory budgeting, and CPU budget measurement before being called production-ready.
5. After all tests pass, finish docs and close the phase.
6. Prefer the shortest clear code that preserves behavior; if shorter code works equally, choose it.

During implementation phases, use focused verification before commits. For web changes, run `npm run build` and `npm run lint` inside `web-tools/unit-editor`. For backend changes, run `./build-and-test.sh`; for STM32H7 work, also run the configured ARM cross-compile once the toolchain target exists.

## Completed Work

### Production Core Refactor

- [x] PA1: Add an explicit APGCore v2 parser boundary that parses YAML strings/files into a raw contract graph without semantic validation.
- [x] PA2: Split unit/project semantic checks into validator modules while preserving current public loader APIs.
- [x] PA3: Introduce a runtime image layer for compact params, signals, state, control metadata, and schedule storage.
- [x] PA4: Move host/tooling introspection toward a measure module that reads runtime/image state without owning DSP execution.
- [x] PA5a: Audit and deprecate v1 runtime/control/unit-loader paths without removing still-tested legacy code.
- [x] PA5b1: Migrate explicit-frame runtime process coverage from v1 runtime to APGCore v2.
- [x] PA5b2: Migrate control transition coverage from v1 ctrl/runtime adapters to APGCore v2.
- [x] PA5b3: Migrate unit fixture load-all smoke coverage from v1 units to APGCore v2.
- [x] PA5b4: Migrate offline chain regression from v1 amp/cab units to an APGCore v2 project runtime.
- [x] PA5b5: Migrate hall reverb/sample-writing regression to an in-memory APGCore v2 pedalboard render.
- [x] PA5b6: Migrate `src/test_runtime.c` from v1 unit/raw-file smoke to an always-built APGCore v2 host smoke.
- [x] PA5b7: Migrate `src/live.c` from v1 runtime/control unit chains to APGCore v2 host-unit chains.
- [x] PA5b8: Split CMake source groups so v1 runtime/control/loader code is not compiled into default v2 targets.
- [x] PA5b9: Remove unused v1 runtime/control/unit-loader source and headers.
- [x] PA5b10: Remove remaining fixed-size unit adapter helpers and their direct adapter test.
- [x] PA5b11a: Remove clean tracked legacy `units/` fixtures that no default build or test references.
- [ ] PA5b11b: Decide whether to keep, port, or delete remaining modified/untracked legacy `units/` files.
- [x] PB1: Audit `src/apgcore` and `inc/apgcore` for remaining boundary leaks between parser, validator, compiler, runtime image, runtime, measure, and host APIs.
- [x] PB2: Move any remaining host/tooling read concerns out of `runtime_v2` into `measure_v2` compatibility wrappers.
- [ ] PB3: Tighten runtime-image ownership so runtime initialization consumes precomputed layout metadata instead of recomputing resource registration.
- [ ] PB4: Add fixed-memory/static-image readiness checks for generated M7 export artifacts.

Module note: Parser v2 now exposes raw YAML contract graphs before validator-specific semantic checks.

Module note: Unit and project validators now own semantic graph checks behind thin parser-backed loaders.

Module note: Runtime image now precomputes layout/defaults/control targets before runtime allocation.

Module note: Measure v2 now exposes runtime snapshots, meters, and diagnostics for host/tooling reads.

Note: v1 code is legacy. Audit, deprecate, and remove unused v1 runtime/unit/YAML paths only after confirming no tests, fixtures, or host tools still depend on them.

Module note: V1 public APIs are now fenced as opt-in deprecated and legacy tests are labelled.

Module note: Explicit-frame runtime capacity coverage now exercises APGCore v2 instead of v1.

Module note: Control transition coverage now exercises APGCore v2 control-port smoothing.

Module note: Unit fixture load-all smoke now exercises APGCore v2 host loading and mono runtime processing.

Module note: Offline chain coverage now runs through a compiled APGCore v2 project runtime.

Module note: Hall render coverage now uses an in-memory APGCore v2 pedalboard runtime path.

Module note: Test runtime smoke now builds without PipeWire and runs through APGCore v2 host loading.

Module note: Live PipeWire playback now loads APGCore v2 units before entering the audio callback.

Module note: Default CMake targets now exclude v1 runtime/control sources and the v1 YAML unit loader.

Module note: V1 runtime/control/unit-loader code has been removed after all default users migrated to APGCore v2.

Module note: Fixed-size unit adapter helpers have been removed; product behavior now lives in v2 unit contracts.

Module note: Clean legacy unit fixtures have been removed; modified local unit files remain untouched.

Module note: Core boundary audit now records remaining runtime/measure/image leaks and moves meter tests to measure APIs.

Module note: Host and tooling tests now read runtime diagnostics and transport state through measure APIs.

Pending tests to record for PB:

- Boundary audit proves parser output can be inspected without semantic validation.
- Runtime initialization from image does not recompute signal, param, control, or schedule layout.
- Measure APIs expose host snapshots without owning or mutating DSP execution state.
- M7 export artifact check rejects dynamic YAML/runtime allocation assumptions.

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
- [x] Refreshed `AGENTS.md` with current repository context, `apply_patch` workflow, `clang-format` approval, and `git commit` approval.

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
- [x] N3: Keep `AGENTS.md`, `task.md`, and this plan aligned when workflow rules change.

### Phase O: Control Routing Semantics

- [x] O1: Define the schema contract for v2 control routing beyond same-named params.
- [x] O2: Add loader validation for explicit control routing targets and rejected unsupported modes.
- [x] O3: Add compiler/runtime support for control-to-param routing metadata.
- [x] O4: Add focused fixtures and runtime tests for routed controls and unsupported control modes.
- [x] O5: Resolve or narrow the remaining non-param control-routing entry in `problem.md`.

### Phase P: Backend Readiness Audit

- [x] P1: Audit current requirements, design notes, task queue, and v2 implementation status.
- [x] P2: Define the web UI readiness checklist and backend handoff gates.
- [x] P3: Add the long-range backend-to-web plan under `docs/plans/`.

### Phase Q: Unit Schema Stabilization

- [x] Q1: Validate `meta` and `ui` sections instead of only tolerating them.
- [x] Q2: Add param UI metadata validation for label, control type, unit, scale, and display precision.
- [x] Q3: Add stable validation paths for UI-facing unit metadata errors.
- [x] Q4: Update schema docs and tests for the finalized UI metadata contract.

### Phase R: Atom Catalog Export

- [x] R1: Define the atom catalog JSON shape needed by the web atom palette.
- [x] R2: Add a backend API, CLI command, or test binary that exports atom metadata.
- [x] R3: Include atom category, in/out/config fields, statefulness, and compatibility profile hints.
- [x] R4: Add regression tests so exported metadata stays aligned with compiler contracts.

### Phase S: Project v2 Schema

- [x] S1: Define `project.v2.yaml` with unit refs, chain nodes, routes, scenes, and target profiles.
- [x] S2: Add small deterministic `projects-v2/` fixtures.
- [x] S3: Validate missing unit refs, duplicate node IDs, bad routes, invalid scene params, and target flags.
- [x] S4: Document project schema limits before implementing broad routing features.

### Phase T: Project Loader and Resolver

- [x] T1: Resolve project-relative unit paths safely.
- [x] T2: Load referenced v2 units into a project model.
- [x] T3: Reject unsafe paths, missing files, and ambiguous references.
- [x] T4: Add multi-file project loader tests.

### Phase U: Project Compiler

- [x] U1: Expand unit instances into namespaced graph nodes, signals, and params.
- [x] U2: Compile inter-unit routes into a single runtime plan.
- [x] U3: Preserve stable instance param names such as `delay1.feedback` for UI/runtime control.
- [x] U4: Add compile/runtime tests for the first project fixture.

### Phase V: CLI and JSON Contract

- [x] V1: Add structured JSON validation output for units and projects.
- [x] V2: Add inspect output for atoms, units, and projects.
- [x] V3: Stabilize the product fixture preview command surface with deterministic project render JSON; benchmark CLI output remains deferred in `problem.md`.
- [x] V4: Commit golden JSON outputs for frontend tests.

### Phase W: Runtime Product Controls

- [x] W1: Implement block-boundary parameter smoothing from `smoothing_ms`.
- [x] W2: Add unit-instance bypass and project-level mute/solo where needed for the first pedalboard workflow.
- [x] W3: Add peak/RMS meter snapshots suitable for UI polling.
- [x] W4: Add runtime tests for live parameter changes, bypass, and meters.

### Phase X: Product Fixture Slice

- [x] X1: Add or migrate v2 units for overdrive, delay, tremolo, EQ/tone stack, noise gate, and wet/dry mix.
- [x] X2: Add a guitar pedalboard project fixture using those units.
- [x] X3: Validate, compile, run, and render the fixture deterministically.
- [x] X4: Capture compatibility and validation outputs for the fixture.

### Phase Y: Web Handoff Package

- [x] Y1: Freeze sample JSON contracts for validation, atom catalog, unit inspect, and project inspect.
- [x] Y2: Update web-readiness docs with exact commands and sample files.
- [x] Y3: Refresh `AGENTS.md`, `task.md`, and this plan for the web UI implementation phase.
- [x] Y4: Declare the backend ready for v2 web UI work.

### Phase Z: Initial Web UI Implementation

- [x] Z1: Audit `web-tools/unit-editor/` and keep the existing app structure that fits the product workflow.
- [x] Z2: Use frozen `test/golden/` JSON as the first frontend data contract and mock data source.
- [x] Z3: Build the project-level pedalboard workflow first: project browser, canvas, routes, parameter panel, and validation inspector.
- [x] Z4: Add atom-level/unit-internals editing only after the project workflow is usable.

### Phase AA: Web UI Refinement and Integration

- [x] AA1: Split the project workbench into focused components without changing behavior.
- [x] AA2: Add UI state for editable parameter controls while keeping frozen samples immutable.
- [x] AA3: Add validation and render command panels that show exact backend commands from `docs/WEB_UI_READINESS.md`.
- [x] AA4: Add a first atom palette/unit-inspection view after the project workflow remains stable.

### Phase AB: Web UI Completion Pass

- [x] AB1: Fix the existing `YamlEditor` hook dependency warning so `npm run lint` is warning-free.
- [x] AB2: Add a project draft export/override preview for edited instance params.
- [x] AB3: Add validation/render readiness states for dirty drafts versus frozen command outputs.
- [x] AB4: Add a compact view switcher for project, atom, and contract inspector sections.

### Phase AC: Contract-Accurate Web Data

- [x] AC1: Freeze a full atom catalog JSON sample from `apg-v2 inspect atoms` and retire the local atom catalog fallback from the web palette.
- [x] AC2: Add a frontend fixture loader shape that treats validation, unit inspect, project inspect, atom catalog, and render JSON as one backend contract bundle.
- [x] AC3: Show compatibility profile fields from backend atom/unit/project metadata instead of local labels.
- [x] AC4: Update `problem.md` when the full atom catalog frontend fixture gap is resolved.

Pending AC tests for Phase AI:

- [ ] Atom palette renders from backend JSON.
- [ ] Local fallback is unused by the project workbench palette.
- [ ] Backend catalog schema mismatch shows a visible failure.

Module note: Web atom data now comes from frozen backend catalog JSON.

### Phase AD: Multi-File Workspace and Autosave

- [x] AD1: Add a web project workspace model for project YAML plus referenced unit files.
- [x] AD2: Add local autosave and restore for draft project/unit files.
- [x] AD3: Add import/export controls for the draft workspace without requiring manual YAML copy/paste.
- [x] AD4: Keep validation/readiness state tied to dirty drafts versus frozen or regenerated backend outputs.

Pending AD tests for Phase AI:

- [ ] Autosave restores project and unit drafts.
- [ ] Export includes all workspace files.
- [ ] Dirty drafts mark validation/render as stale.

Module note: Project drafts now preserve multi-file workspace state locally.

### Phase AE: Unit Graph Editing

- [x] AE1: Add a unit-internals view that renders atom nodes, signals, bindings, and params.
- [x] AE2: Add atom insertion/config editing against backend atom metadata.
- [x] AE3: Add route/binding validation feedback before applying structural graph edits.
- [x] AE4: Export updated unit draft YAML through the same workspace model.

Pending AE tests for Phase AI:

- [ ] Unit graph renders from unit inspect data.
- [ ] Atom config edits update draft YAML.
- [ ] Invalid binding shows inspector feedback.

Module note: Unit graph edits now flow through draft workspace YAML.

### Phase AF: Live Preview Path

- [x] AF1: Define the browser preview adapter boundary for compile, start, stop, set param, bypass, and meter polling.
- [x] AF2: Add a preview panel state machine using deterministic render JSON until WASM/AudioWorklet is available.
- [x] AF3: Wire meter and param controls to the runtime-control contract names.
- [x] AF4: Track missing WASM/AudioWorklet implementation gaps in `problem.md` if backend support is not ready.

Pending AF tests for Phase AI:

- [ ] Preview panel transitions through idle/ready/running/error.
- [ ] Param and bypass controls call stable runtime names.
- [ ] Meter display handles missing preview backend.

Module note: Preview controls now target the runtime adapter contract.

### Phase AG: Compatibility and Export Workflow

- [x] AG1: Add a compatibility matrix UI for `desktop_full`, `wasm_realtime`, `m7_static`, and `offline_render`.
- [x] AG2: Add export readiness panels for desktop/web/embedded targets.
- [x] AG3: Surface benchmark/export command gaps from `problem.md` as blocked export actions.
- [x] AG4: Add backend CLI/export tasks when the UI needs real generated bundles.

Pending AG tests for Phase AI:

- [ ] Matrix shows supported/unsupported target profiles.
- [ ] Export panel blocks unavailable targets with reason.
- [ ] M7 export path rejects unsupported atoms/features.

Module note: Export UI now exposes target readiness and blocked backend gaps.

### Phase AH: Backend CLI and Export Gaps

- [x] AH1: Add deterministic benchmark JSON only if needed by export/readiness UI.
- [x] AH2: Add export command skeletons for `wasm_realtime` and `m7_static`.
- [x] AH3: Keep `apgcore` independent from platform APIs while adding export surfaces.
- [x] AH4: For M7 static output, generate C11-compatible tables with bounded memory and no runtime YAML parser.

Pending AH tests for Phase AI:

- [ ] Benchmark JSON has stable structural fields.
- [ ] M7 export emits deterministic static bundle.
- [ ] Unsupported target features produce stable diagnostics.

Module note: Backend CLI now exposes MVP export surfaces.

### Phase AI: Deferred Test Implementation

- [x] AI1: Implement recorded Phase AC tests.
- [x] AI2: Implement recorded Phase AD tests.
- [x] AI3: Implement recorded Phase AE tests.
- [x] AI4: Implement recorded Phase AF tests.
- [x] AI5: Implement recorded Phase AG/AH tests.
- [x] AI6: Run package build/lint for web and `./build-and-test.sh` for C/APGCore.

### Phase AJ: Final Docs and Goal Closure

- [x] AJ1: Update `task.md`, `plan.md`, `problem.md`, and relevant schema/readiness docs.
- [x] AJ2: Document remaining non-MVP follow-ups separately.
- [x] AJ3: Confirm no unrelated local files are staged.
- [x] AJ4: Commit final docs slice.

## Execution Order

1. Stabilize unit UI metadata before exporting frontend-facing unit data.
2. Export atom catalog metadata before building the project compiler UI contract.
3. Add project schema and loader before compiling multi-unit pedalboards.
4. Stabilize JSON/CLI contracts before starting web UI implementation.
5. Prove the workflow with a guitar pedalboard fixture before moving to frontend screens.
6. Start web UI work from frozen backend samples before adding live backend integration.
7. Complete AC-AH implementation modules before adding deferred tests in Phase AI.
8. Finish final documentation and goal closure in Phase AJ.

## Per-Phase Workflow

1. Pick one checkbox or a tight group of related checkboxes.
2. Implement the shortest clear code change that preserves intended behavior.
3. Format touched C/H files with `clang-format` when source layout changes.
4. During AC-AH, record pending tests instead of implementing them.
5. Run build-only verification for implementation slices.
6. Commit with `git commit -m "<which tasks are done>"`.
