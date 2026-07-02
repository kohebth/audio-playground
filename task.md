# Continuous Implementation Tasks

## Current Focus

APGCore v2 MVP implementation work tracked in `plan.md` is complete through phases AC-AJ. Validation/inspect/render/benchmark JSON, frozen sample contracts, project workspace editing, deterministic preview, compatibility/export UI, M7 static export surfaces, deferred tests, and final docs are implemented.

Current production focus: move beyond MVP surfaces toward a hardware-ready multi-effect core. The target architecture in `core-design.md` isolates `metadata`, `parser`, `validator`, `compiler`, runtime image, `runtime`, `measure`, and `host` so parsing, validation, graph expansion, resource registration, and measurement stay out of the real-time execution path.

Status: Phase 0 atom migration, Phase 1 explicit-frame adapters, APGCore v2 phases H through AJ, and the MVP web/backend handoff are complete. The project is not ready for STM32H7/M7 production yet. Next work should first make the core boundaries production-shaped, then continue STM32H7/M7 export validation, cross-compilation, audio callback integration, memory/CPU budgeting, and real WASM AudioWorklet preview/export.

## Production Core Refactor Queue

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
- [x] PA5b11b: Decide whether to keep, port, or delete remaining modified/untracked legacy `units/` files.
- [x] PB1: Audit `src/apgcore` and `inc/apgcore` for remaining boundary leaks between parser, validator, compiler, runtime image, runtime, measure, and host APIs.
- [x] PB2: Move any remaining host/tooling read concerns out of `runtime_v2` into `measure_v2` compatibility wrappers.
- [x] PB3: Tighten runtime-image ownership so runtime initialization consumes precomputed layout metadata instead of recomputing resource registration.
- [x] PB4: Add fixed-memory/static-image readiness checks for generated M7 export artifacts.
- [x] PB3b: Move signal-binding and mix-matrix structured-config resolution to runtime-image plans consumed at runtime init.
- [x] PB5: Add host-level project orchestration APIs for resolved-project load, compile, runtime-image build, and mono runtime transport.

Module note: Parser v2 now exposes raw YAML contract graphs before validator-specific semantic checks.

Module note: Unit and project validators now own semantic graph checks behind thin parser-backed loaders.

Module note: Runtime image now precomputes layout/defaults/control targets before runtime allocation.

Module note: Runtime no longer owns or mutates meter snapshots at process time; measure reads port meters from signal buffers.

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

Module note: Remaining legacy `units/*.unit.yaml` files have been removed after confirming no production path depends on v1 fixture loading.

Module note: Core boundary audit now records remaining runtime/measure/image leaks and moves meter tests to measure APIs.

Module note: Host and tooling tests now read runtime diagnostics and transport state through measure APIs.

Module note: Runtime image now owns per-node storage layout consumed by runtime initialization.
Module note: Runtime initialization now consumes runtime-image signal-binding and mix-matrix config plans instead of resolving descriptors.
Module note: Delay-tap input field metadata now comes from atom registry lookup, not atom-name branching in runtime-image.
Module note: `apg_v2_host_project_*` now exposes production project orchestration from resolved project load through runtime initialization and mono processing.

Module note: M7 export tests now reject generated dynamic allocation, YAML, loader, and runtime-init dependencies.

Pending tests to record for PB:

- Boundary audit proves parser output can be inspected without semantic validation.
- Runtime initialization from image does not recompute signal, param, control, or schedule layout.
- Measure APIs expose host snapshots without owning or mutating DSP execution state.
- M7 export artifact check rejects dynamic YAML/runtime allocation assumptions.

## STM32H7/M7 Readiness Queue

- [x] PC1: Add deterministic M7 static memory manifest fields to export JSON and generated headers.
- [x] PC2: Add configurable M7 memory budgets and reject generated bundles that exceed them.
- [x] PC3: Add an ARM/M7 cross-compile gate for generated bundle syntax when a toolchain is configured.
- [x] PC4: Define the board audio callback integration contract for fixed block processing, DMA ownership, and cache coherency.

Module note: M7 export now reports deterministic static RAM byte counts for generated bundles.

Module note: M7 export now rejects bundles that exceed an explicit static RAM budget.

Module note: M7 export syntax is now checked by an ARM/M7 compiler when configured.

Module note: STM32H7 board integration now has a fixed-block DMA/cache contract.

## Runtime Image Hardening Queue

- [x] PD1: Move per-node state-buffer sample capacities into runtime image metadata.
- [x] PD2: Move signal-array auxiliary allocation sizing into runtime image metadata.
- [x] PD3: Precompute scalar config/input refresh plans so runtime processing does not scan binding keys.

Module note: Runtime image now owns state-buffer capacity layout for runtime allocation.

Module note: Runtime image now sizes signal-array pointer pools for runtime binding.

Module note: Runtime processing now refreshes scalar fields from image plans.

## Runtime Memory Hardening Queue

- [x] PE1: Move atom out/in/config/state call storage into one runtime-image-planned contiguous pool.
- [x] PE2: Move state buffer allocations toward one state-buffer pool using image offsets.
- [x] PE3: Add static export memory sections for atom storage, signal buffers, params, and state buffers.

Module note: Runtime atom call storage now uses one image-planned contiguous pool.

Module note: Runtime state buffers now use one image-planned contiguous pool.

Module note: M7 export now declares section-placed runtime memory buffers.

Implementation lifecycle for production phases:

1. Implement as much as possible module-by-module.
2. Record pending tests next to production tasks before broad implementation.
3. After each completed implementation module, add a module note of 40 words or fewer.
4. For hardware-facing work, verify with generated artifacts, cross-compile checks, and fixed memory/CPU budgets before calling it ready.
5. After all tests pass, finish docs and close the production phase.
6. Prefer the shortest clear code that preserves behavior; if shorter code works equally, choose it.

Verification policy for future work: run focused tests for the changed area. For web changes, use `npm run test`, `npm run build`, and `npm run lint` inside `web-tools/unit-editor`. For backend changes, use `./build-and-test.sh`.

## Completed Foundation

- [x] Add `inc/apgcore/process.h` with `apg_process_info_t`.
- [x] Extend `atom_call_t` to carry process metadata.
- [x] Pass runtime `sample_rate`, `chunk_length`, and channel metadata into atom thunks.
- [x] Add variable-frame tests for 64, 128, 256, 512, and 1024 frame buffers.
- [x] Migrate initial atom slices:
  - [x] Core: `amplitude_multiply`, `amplitude_clip_soft`, `delay_line`, `filter_biquad`
  - [x] Modulation/mix: `generation_dc`, `generation_lfo`, `mix_wet_dry`, `modulation_amplitude`
  - [x] Control/detect: `amplitude_smooth`, `detect_envelope`, `detect_peak`, `detect_threshold`
  - [x] Delay/stateful: `delay_unit`, `delay_fractional`, `delay_tap_feedback`, `delay_tap_feedforward`
  - [x] Arithmetic/mix: `amplitude_add`, `amplitude_subtract`, `amplitude_divide`, `mix_crossfade`
  - [x] Amplitude/state: `amplitude_clip_hard`, `amplitude_accumulate`, `amplitude_latch`, `amplitude_normalize`
  - [x] Simple generators: `generation_impulse`, `generation_noise`, `generation_envelope`, `generation_oscillator`
  - [x] Remaining modulation: `modulation_ring`, `modulation_frequency`, `modulation_phase`, `modulation_scrub`
  - [x] Remaining detectors: `detect_slope`, `detect_rms`, `detect_zero_crossing`, `detect_autocorrelate`, `detect_pitch`
  - [x] Remaining filters: `filter_allpass`, `filter_comb_ff`, `filter_comb_fb`, `filter_dc_block`, `filter_differentiate`, `filter_integrate`, `filter_fir`
  - [x] Remaining mix/nonlinear: `mix_matrix`, `mix_pan_stereo`, `mix_encode_ms`, `mix_decode_ms`, `nonlinear_bitcrush`, `nonlinear_waveshape`, `nonlinear_samplerate_reduce`
  - [x] Remaining interpolation/SRC-safe: `interpolation_linear`, `interpolation_cubic`, `interpolation_lagrange`, `interpolation_sinc`, `src_convert_format`, `src_antialias`, `src_antiimage`
  - [x] Remaining spectral-safe: `freq_window`, `freq_quantize`
  - [x] Remaining ratio-based SRC-safe: `src_downsample`
  - [x] Remaining ratio-based SRC-safe: `src_upsample` with `output_frames` capacity metadata.
  - [x] Remaining spectral-safe: `freq_shift`
- [x] Remaining overlap/history-safe: `freq_overlap_add`, `freq_overlap_save`
- [x] Remove atom-level `CHUNK_LENGTH` leftovers from `mix_matrix` and the unused `freq_quantize` define.
- [x] Split `test/test_atom_basic.c` into smaller translation units with a shared harness and a thin `main` driver.

## Next Slice

- [x] Review and migrate the remaining fixed-size spectral and SRC atoms:
  - [x] Add `output_frames` capacity metadata to `apg_process_info_t`.
  - [x] Migrate `src_upsample` to `src_upsample_process(...)` with output-capacity clamping.
  - [x] Keep `src/live.c` and `src/unit` fixed-size adapters in Phase 0.

## Later Slices

- [x] Decide whether `src/live.c` and `src/unit/*.c` should remain fixed-size adapters or become variable-frame users. Keep them fixed-size adapters in Phase 0; they depend on the 512-frame runtime contract and lack output-capacity metadata for a safe variable-frame migration.

## Next Phase Candidates

- [x] Define the initial `unit.v2.yaml` schema boundary in `docs/schemas/unit-v2.md` and add `units-v2/simple_gain.unit.v2.yaml` as the first compiler fixture.
- [x] Add a minimal `apg_unit_v2_load_*` validator with CTest coverage for the simple gain fixture and invalid schema cases.
- [x] Extend `apg_unit_v2_load_*` to expose parsed params, ports, signals, nodes, and bindings for the next compiler slice.
- [x] Add `apg_v2_compile_unit(...)` to resolve source-order schedule nodes, atom entries, signal bindings, and parameter config bindings for `simple_gain`.
- [x] Add MVP compiler binding-key validation for `generation_dc` and `amplitude_multiply` so misspelled in/out/config keys fail at compile time.
- [x] Add MVP compiler dataflow validation so nodes cannot read unproduced signals and public output signals must be produced.
- [x] Reject duplicate v2 graph signal names during unit loading to keep signal-to-index resolution unambiguous.
- [x] Align v2 port validation with the schema: allow `audio` and `control`, require channels/signals only for audio ports, and skip control ports during compiler signal checks.
- [x] Align v2 param validation with the schema: allow `float`, `int`, and `bool`, and require `min`/`max` only for numeric params.
- [x] Reject duplicate v2 param and port names during unit loading to keep public name resolution unambiguous.
- [x] Reject duplicate v2 node binding keys during unit loading so atom binding resolution is unambiguous.
- [x] Validate v2 `compatibility` as a non-empty map of boolean target flags.
- [x] Add MVP topological scheduling in the v2 compiler so forward node references are reordered and unresolved dependencies still fail.
- [x] Phase 1 runtime sizing: add `runtime_unit_process_frames(...)` so processing can use an explicit frame count up to the allocated `chunk_length` capacity while preserving `runtime_unit_process(...)` compatibility.
- [x] Add CTest coverage for explicit-frame runtime processing with sentinel checks and over-capacity rejection.
- [x] Phase 1 live/control adapter: add `ctrl_unit_process_frames(...)` and pass live playback chunk length through to the runtime.
- [x] Phase 1 unit adapter migration: add frame-aware `chorus_process_frames(...)` and `sustainer_process_frames(...)` with per-call scratch allocation and focused CTest coverage.
- [x] Stabilization: inspect the changed runtime/control/unit adapter surface and keep the CMake/CTest suite green before starting schema/compiler work.

## Next Work Queue

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
- [x] Y3: Refresh `AGENTS.md`, `task.md`, and `plan.md` for the web UI implementation phase.
- [x] Y4: Declare the backend ready for v2 web UI work.

## Web UI Implementation Queue

- [x] Z1: Audit `web-tools/unit-editor/` and choose the existing app entry points to keep.
- [x] Z2: Load frozen backend samples from `test/golden/` as initial UI fixture data.
- [x] Z3: Build the project-level pedalboard browser, canvas, and route view around `projects-v2/guitar-pedalboard.project.v2.yaml`.
- [x] Z4: Add parameter and validation inspectors from unit/project inspect JSON before adding unit-internals editing.

## Next Web UI Queue

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

### Phase AD: Multi-File Project Editing and Autosave

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

- [x] AE1: Add a unit-internals view that renders atom nodes, signals, and bindings from unit inspect data.
- [x] AE2: Add atom insertion/config editing against backend atom metadata.
- [x] AE3: Add route/binding validation feedback before applying structural graph edits.
- [x] AE4: Export updated unit draft YAML through the same workspace model.

Pending AE tests for Phase AI:

- [ ] Unit graph renders from unit inspect data.
- [ ] Atom config edits update draft YAML.
- [ ] Invalid binding shows inspector feedback.

Module note: Unit graph edits now flow through draft workspace YAML.

### Phase AF: Live Preview Path

- [x] AF1: Define the browser preview API boundary for compile, start/stop, set param, bypass, and meter polling.
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

## Completed Work Queue

### Batch A: Compiler Contracts

- [x] Add required-binding validation for MVP atoms so missing `in`, `out`, or `config` keys fail with clear errors.
- [x] Extend the compiler binding metadata beyond `generation_dc` and `amplitude_multiply` for the next simple atoms: `amplitude_add`, `amplitude_subtract`, `amplitude_clip_*`, and `mix_wet_dry`.
- [x] Store node output producer indexes in the compiled plan so later runtime execution can bind buffers without re-scanning nodes.
- [x] Add focused compile tests for missing required bindings, extra bindings, and multi-node topological ordering.

### Batch B: v2 Execution MVP

- [x] Define an `apg_v2_runtime_t` or equivalent execution context that owns signal buffers, param values, and atom call storage.
- [x] Add a runtime init function from `apg_v2_compiled_unit_t` that allocates buffers for audio signals and initializes param defaults.
- [x] Implement a mono `simple_gain` execution path using the compiled schedule and existing atom registry thunks.
- [x] Add a CTest that runs `units-v2/simple_gain.unit.v2.yaml` over a small buffer and verifies gain output samples.

### Batch C: Schema Fixtures

- [x] Add `units-v2/simple_mix.unit.v2.yaml` using two inputs and `amplitude_add` or `mix_wet_dry`.
- [x] Add `units-v2/simple_clip.unit.v2.yaml` using gain plus clipping to exercise literal config and chained processing.
- [x] Add loader/compile fixture tests that iterate all `units-v2/*.yaml` files.
- [x] Keep fixtures intentionally small and deterministic; avoid large generated audio files.

### Batch D: Error Quality

- [x] Include node IDs, binding section names, and binding keys in v2 compiler error messages.
- [x] Include param/port/signal names in loader validation errors when rejecting duplicates or invalid fields.
- [x] Add tests that assert representative error messages contain the useful identifier.

### Batch E: Tooling and Docs

- [x] Refresh `docs/schemas/unit-v2.md` to reflect implemented validation rules and current MVP limitations.
- [x] Add a short v2 compiler architecture note covering loader, compiler plan, topological scheduling, and future runtime execution.
- [x] Update `AGENTS.md` if build/test or v2 workflow conventions change. No update was needed for this batch.
- [x] Keep using `./build-and-test.sh` once per completed implementation slice.

### Batch F: Runtime Generalization

- [x] Add v2 runtime helpers to find signal buffers and set params by public name.
- [x] Add a generic `apg_v2_runtime_process(...)` schedule executor independent of mono I/O copying.
- [x] Refactor `apg_v2_runtime_process_mono(...)` onto the generic executor.
- [x] Add runtime tests for `simple_clip` using literal config and chained processing.
- [x] Add runtime support/tests for multi-input mono fixtures such as `simple_mix`.

### Batch G: Runtime Robustness

- [x] Allocate atom state `FIELD_BUFFER` descriptors for v2 runtime nodes that need scratch/history buffers.
- [x] Add runtime cleanup tests for partially failed initialization paths.
- [x] Add error reporting hooks so v2 runtime process failures expose a useful message instead of only `false`.

## Per-Slice Workflow

1. Pick one batch item or a tight group of related batch items.
2. Implement only the files needed for that batch item.
3. Add focused CTest coverage before broadening behavior.
4. Format touched C/H files with `clang-format` when source layout changes.
5. Verify once with `./build-and-test.sh`.
6. Commit completed task slices with `git commit -m "<which tasks are done>"`.

Legacy atom migration checklist, retained for future atom work:

1. Pick a small atom group with similar behavior.
2. Add `*_process(..., const apg_process_info_t *info)` declarations to `inc/atom/dsp_atoms.h`.
3. Implement each process function using `apg_process_frames_or_default(info)`.
4. Preserve the existing wrapper and call the new process function with `apg_process_info_default()`.
5. Replace generated registry macro entries with explicit thunks for migrated atoms.
6. Add sentinel-based variable-frame tests.
7. Format touched C/H files with `clang-format`.
8. Verify:

```sh
cmake --build /tmp/audio-playground-apgcore-build --target test_atom_basic
/tmp/audio-playground-apgcore-build/test_atom_basic
cmake --build /tmp/audio-playground-apgcore-build
ctest --test-dir /tmp/audio-playground-apgcore-build
```

## Guardrails

- Do not rewrite YAML schemas or introduce the v2 compiler in Phase 0.
- Do not remove existing atom APIs until all callers are migrated.
- Keep changes scoped to one logical atom slice per pass.
- Treat the existing `build/` directory as stale; use `/tmp/audio-playground-apgcore-build` for verification.
