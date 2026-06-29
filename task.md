# Continuous Implementation Tasks

## Current Focus

APGCore v2 implementation work tracked in `plan.md` is complete through Phase P.

Goal: prepare stable backend contracts so the v2 web UI can consume unit metadata, atom catalog data, project files, validation output, runtime controls, and product fixtures without depending on changing C internals.

Status: Phase 0 atom migration, Phase 1 explicit-frame adapters, and APGCore v2 phases H through P are complete. Use `./build-and-test.sh` for full verification on code/test slices, `cmake --build /tmp/audio-playground-apgcore-build --target check_v2` for focused v2 checks, and the sanitizer CMake option documented in `AGENTS.md` for debug verification. Docs-only slices do not require the build wrapper.

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

- [ ] Q1: Validate `meta` and `ui` sections instead of only tolerating them.
- [ ] Q2: Add param UI metadata validation for label, control type, unit, scale, and display precision.
- [ ] Q3: Add stable validation paths for UI-facing unit metadata errors.
- [ ] Q4: Update schema docs and tests for the finalized UI metadata contract.

### Phase R: Atom Catalog Export

- [ ] R1: Define the atom catalog JSON shape needed by the web atom palette.
- [ ] R2: Add a backend API, CLI command, or test binary that exports atom metadata.
- [ ] R3: Include atom category, in/out/config fields, statefulness, and compatibility profile hints.
- [ ] R4: Add regression tests so exported metadata stays aligned with compiler contracts.

### Phase S: Project v2 Schema

- [ ] S1: Define `project.v2.yaml` with unit refs, chain nodes, routes, scenes, and target profiles.
- [ ] S2: Add small deterministic `projects-v2/` fixtures.
- [ ] S3: Validate missing unit refs, duplicate node IDs, bad routes, invalid scene params, and target flags.
- [ ] S4: Document project schema limits before implementing broad routing features.

### Phase T: Project Loader and Resolver

- [ ] T1: Resolve project-relative unit paths safely.
- [ ] T2: Load referenced v2 units into a project model.
- [ ] T3: Reject unsafe paths, missing files, and ambiguous references.
- [ ] T4: Add multi-file project loader tests.

### Phase U: Project Compiler

- [ ] U1: Expand unit instances into namespaced graph nodes, signals, and params.
- [ ] U2: Compile inter-unit routes into a single runtime plan.
- [ ] U3: Preserve stable instance param names such as `delay1.feedback` for UI/runtime control.
- [ ] U4: Add compile/runtime tests for the first project fixture.

### Phase V: CLI and JSON Contract

- [ ] V1: Add structured JSON validation output for units and projects.
- [ ] V2: Add inspect output for atoms, units, and projects.
- [ ] V3: Add or stabilize render/benchmark command surfaces for product fixtures.
- [ ] V4: Commit golden JSON outputs for frontend tests.

### Phase W: Runtime Product Controls

- [ ] W1: Implement block-boundary parameter smoothing from `smoothing_ms`.
- [ ] W2: Add unit-instance bypass and project-level mute/solo where needed for the first pedalboard workflow.
- [ ] W3: Add peak/RMS meter snapshots suitable for UI polling.
- [ ] W4: Add runtime tests for live parameter changes, bypass, and meters.

### Phase X: Product Fixture Slice

- [ ] X1: Add or migrate v2 units for overdrive, delay, tremolo, EQ/tone stack, noise gate, and wet/dry mix.
- [ ] X2: Add a guitar pedalboard project fixture using those units.
- [ ] X3: Validate, compile, run, and render the fixture deterministically.
- [ ] X4: Capture compatibility and validation outputs for the fixture.

### Phase Y: Web Handoff Package

- [ ] Y1: Freeze sample JSON contracts for validation, atom catalog, unit inspect, and project inspect.
- [ ] Y2: Update web-readiness docs with exact commands and sample files.
- [ ] Y3: Refresh `AGENTS.md`, `task.md`, and `plan.md` for the web UI implementation phase.
- [ ] Y4: Declare the backend ready for v2 web UI work.

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
