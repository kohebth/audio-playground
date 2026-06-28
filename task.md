# Continuous Implementation Tasks

## Current Focus

Continue **APGCore Phase 0 Foundation** from `docs/plans/2026-06-28-apgcore-phase-0-foundation-plan.md`.

Goal: move the C DSP runtime and atoms away from fixed `CHUNK_LENGTH` assumptions by routing processing through `apg_process_info_t`, while preserving the existing v1 atom wrapper API and YAML runtime behavior.

Status: Phase 0 atom migration is complete. Phase 1 has added explicit-frame runtime/control/unit adapter entry points while preserving the v1 `chunk_length` capacity contract.

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
- [x] Phase 1 runtime sizing: add `runtime_unit_process_frames(...)` so processing can use an explicit frame count up to the allocated `chunk_length` capacity while preserving `runtime_unit_process(...)` compatibility.
- [x] Add CTest coverage for explicit-frame runtime processing with sentinel checks and over-capacity rejection.
- [x] Phase 1 live/control adapter: add `ctrl_unit_process_frames(...)` and pass live playback chunk length through to the runtime.
- [x] Phase 1 unit adapter migration: add frame-aware `chorus_process_frames(...)` and `sustainer_process_frames(...)` with per-call scratch allocation and focused CTest coverage.
- [x] Stabilization: inspect the changed runtime/control/unit adapter surface and keep the CMake/CTest suite green before starting schema/compiler work.

## Per-Slice Workflow

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
