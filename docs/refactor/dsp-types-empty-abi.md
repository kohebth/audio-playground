# DSP Empty-Type ABI Transition

## Decision

This document describes the historical GNU-to-phase-1 transition. Later execution-API work replaced the reserved
`src_downsample_state_t` and `src_upsample_state_t` layouts with explicit `uint32_t phase` state; all other entries in
the table remain the phase-1 evidence set.

The 46 zero-member atom structures from the GCC baseline are now distinct standard C11 structures containing one
`uint8_t _reserved` member. Distinct public typedefs are retained so atom-specific input, parameter, and state types do
not become C-compatible aliases of one shared empty type.

This is an intentional ABI exception. Under GCC 13.3.0 on LP64, each affected type changes from size 0, alignment 1 to
size 1, alignment 1, with `_reserved` at offset 0. All 240 non-empty atom structures, shared port layouts, pointer
aliases, field offsets, and enum values remain unchanged. Consumers must rebuild rather than exchange affected structs
with objects compiled against the GNU zero-size layout.

## Affected Types

| Family | Types |
|---|---|
| amplitude | `amplitude_accumulate_params_t`, `amplitude_add_params_t`, `amplitude_add_state_t`, `amplitude_clip_hard_state_t`, `amplitude_clip_soft_state_t`, `amplitude_divide_state_t`, `amplitude_multiply_params_t`, `amplitude_multiply_state_t`, `amplitude_subtract_params_t`, `amplitude_subtract_state_t` |
| delay | `delay_tap_feedback_state_t`, `delay_tap_feedforward_state_t`, `delay_unit_params_t` |
| detect | `detect_slope_params_t`, `detect_threshold_state_t`, `detect_zero_crossing_params_t` |
| filter | `filter_differentiate_params_t`, `filter_integrate_params_t` |
| frequency | `freq_multiply_state_t`, `freq_window_state_t` |
| generation | `generation_dc_in_t`, `generation_dc_state_t`, `generation_impulse_in_t`, `generation_lfo_in_t`, `generation_noise_in_t` |
| interpolation | `interpolation_cubic_params_t`, `interpolation_cubic_state_t`, `interpolation_linear_params_t`, `interpolation_linear_state_t` |
| mix | `mix_crossfade_state_t`, `mix_decode_ms_params_t`, `mix_decode_ms_state_t`, `mix_encode_ms_params_t`, `mix_encode_ms_state_t`, `mix_matrix_state_t`, `mix_pan_stereo_state_t`, `mix_wet_dry_state_t` |
| modulation | `modulation_amplitude_state_t`, `modulation_ring_params_t`, `modulation_ring_state_t`, `modulation_scrub_state_t` |
| nonlinear | `nonlinear_bitcrush_state_t`, `nonlinear_waveshape_state_t` |
| src | `src_convert_format_state_t`, `src_downsample_state_t`, `src_upsample_state_t` |

`freq_quantize_params_t` and `freq_quantize_state_t` remain non-empty four-byte structures with their existing `unused`
members. Their pre-existing mismatch with zero canonical config/state fields is outside this portability change.

## Runtime Impact

- `src/rte/atom_register.c` now reports size 1 for affected public layouts, matching standard C `sizeof`.
- `src/apgcore/registry/registry_v2.c` already normalized every zero-byte atom layout to one byte through
  `atom_storage_size`. Therefore arena allocation sizes, offsets, and pointer validity are unchanged by the transition.
- Canonical config/state field counts and field descriptors remain unchanged. `_reserved` is ABI storage, not a user
  parameter, persisted state field, or catalog control.
- Atom catalog `sizes` now report the real one-byte layouts. The logical `stateful` value ignores one-byte reserved
  state and retains the existing opaque `freq_quantize` behavior.
- Repository searches found no raw atom-structure file or network serialization, no arrays or pointer arithmetic over
  empty atom types, and no structure-tag forward declarations. WASM/M7 images serialize planned layout sizes and field
  data, not raw compiler-dependent parameter/state objects.

## Enforcement

`test/abi/dsp_types_abi_baseline_lp64.csv` is the immutable GNU-layout baseline.
`test/abi/dsp_types_abi_phase1_lp64.csv` freezes this transition and
`test/abi/dsp_types_abi_c11_lp64.csv` is the current C11 reference. The `test_dsp_types_abi` CTest requires an exact
match to the current reference, then independently compares phase 1 with the GNU baseline and rejects every difference
except exactly 46 size-0-to-size-1 transitions plus their `_reserved` offset-zero fields.

The transition passes:

- GCC C11 standalone headers with `-Wall -Wextra -Wpedantic -Werror`;
- the C++17 public-header smoke test;
- Arm GNU 13.2.1 Cortex-M7 freestanding C11 with the same warning policy;
- the Debug AddressSanitizer/UndefinedBehaviorSanitizer build and all 70 CTest tests.
