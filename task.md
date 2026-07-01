# Continuous Implementation Tasks

## Current Focus

APGCore v2 implementation work tracked in `plan.md` is complete through Phase Y4. Phase V validate/inspect JSON contracts, deterministic project render JSON, fixture output capture, frozen sample contracts, documented handoff commands, web UI guidance refresh, final readiness declaration, and runtime product controls are implemented. The benchmark surface remains tracked separately as a nonblocking follow-up.

Previous goal achieved: stable backend contracts are ready for the v2 web UI to consume unit metadata, atom catalog data, project files, validation output, runtime controls, and product fixtures without depending on changing C internals.

Status: Phase 0 atom migration, Phase 1 explicit-frame adapters, APGCore v2 phases H through Y, and Phase V validate/inspect/render JSON contracts are complete. Phase Z initial web UI work, Phase AA refinement, and Phase AB completion pass are complete. The next target is the full v2 MVP from `audio-playground-v2-requirements.md` and `audio-playground-v2-design.md`: contract-accurate multi-file web editing, local autosave, unit-internals editing, live preview path, compatibility/export workflow, and remaining backend CLI/export/adapter gaps. Use package-local web commands inside `web-tools/unit-editor/` for frontend slices.

Implementation lifecycle for the full v2 MVP:

1. Implement as much as possible module-by-module.
2. After each completed implementation module, record pending test cases here and in `plan.md`; do not implement those tests until Phase AI.
3. After each completed implementation module, add a module note of 40 words or fewer.
4. After all implementation modules are complete, implement and run tests.
5. After all tests pass, finish docs and close the goal.
6. Prefer the shortest clear code that preserves behavior; if shorter code works equally, choose it.

Verification policy during implementation phases: run build-only verification before each implementation commit. For web changes, run `npm run build` inside `web-tools/unit-editor`. For backend changes, run `./build-and-test.sh` only when backend implementation changes require compile confidence. Do not add new test cases before Phase AI.

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

- [ ] AH1: Add deterministic benchmark JSON only if needed by export/readiness UI.
- [ ] AH2: Add export command skeletons for `wasm_realtime` and `m7_static`.
- [ ] AH3: Keep `apgcore` independent from platform APIs while adding export surfaces.
- [ ] AH4: For M7 static output, generate C11-compatible tables with bounded memory and no runtime YAML parser.

### Phase AI: Deferred Test Implementation

- [ ] AI1: Implement recorded Phase AC tests.
- [ ] AI2: Implement recorded Phase AD tests.
- [ ] AI3: Implement recorded Phase AE tests.
- [ ] AI4: Implement recorded Phase AF tests.
- [ ] AI5: Implement recorded Phase AG/AH tests.
- [ ] AI6: Run package build/lint for web and `./build-and-test.sh` for C/APGCore.

### Phase AJ: Final Docs and Goal Closure

- [ ] AJ1: Update `task.md`, `plan.md`, `problem.md`, and relevant schema/readiness docs.
- [ ] AJ2: Document remaining non-MVP follow-ups separately.
- [ ] AJ3: Confirm no unrelated local files are staged.
- [ ] AJ4: Commit final docs slice.

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
