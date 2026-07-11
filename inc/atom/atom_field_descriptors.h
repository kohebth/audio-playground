#ifndef ATOM_FIELD_DESCRIPTORS_H
#define ATOM_FIELD_DESCRIPTORS_H

#include <atom_registry.h>

#define APG_ATOM_FIELD_TABLES(_)                   \
    _(amplitude_accumulate_state_fields, 1)        \
    _(amplitude_clip_hard_config_fields, 1)        \
    _(amplitude_clip_soft_config_fields, 2)        \
    _(amplitude_divide_config_fields, 1)           \
    _(amplitude_latch_config_fields, 1)            \
    _(amplitude_latch_state_fields, 2)             \
    _(amplitude_normalize_config_fields, 2)        \
    _(amplitude_normalize_state_fields, 1)         \
    _(amplitude_smooth_config_fields, 3)           \
    _(amplitude_smooth_state_fields, 1)            \
    _(delay_fractional_config_fields, 2)           \
    _(delay_fractional_state_fields, 3)            \
    _(delay_line_config_fields, 1)                 \
    _(delay_line_state_fields, 3)                  \
    _(delay_tap_feedback_config_fields, 1)         \
    _(delay_tap_feedforward_config_fields, 1)      \
    _(delay_tap_feedback_in_fields, 2)             \
    _(delay_tap_feedforward_in_fields, 2)          \
    _(delay_unit_state_fields, 1)                  \
    _(detect_autocorrelate_config_fields, 1)       \
    _(detect_autocorrelate_state_fields, 3)        \
    _(detect_pitch_config_fields, 2)               \
    _(detect_pitch_state_fields, 3)                \
    _(detect_envelope_config_fields, 3)            \
    _(detect_envelope_state_fields, 1)             \
    _(detect_peak_config_fields, 3)                \
    _(detect_peak_state_fields, 1)                 \
    _(detect_rms_config_fields, 1)                 \
    _(detect_rms_state_fields, 4)                  \
    _(detect_slope_state_fields, 1)                \
    _(detect_threshold_config_fields, 1)           \
    _(detect_zero_crossing_state_fields, 1)        \
    _(filter_allpass_config_fields, 2)             \
    _(filter_allpass_state_fields, 3)              \
    _(filter_biquad_coefficients_config_fields, 5) \
    _(filter_biquad_config_fields, 5)              \
    _(filter_biquad_in_fields, 2)                  \
    _(filter_biquad_state_fields, 4)               \
    _(filter_biquad_coefficients_state_fields, 2)  \
    _(filter_comb_fb_config_fields, 2)             \
    _(filter_comb_fb_state_fields, 3)              \
    _(filter_comb_ff_config_fields, 2)             \
    _(filter_comb_ff_state_fields, 3)              \
    _(filter_dc_block_config_fields, 1)            \
    _(filter_dc_block_state_fields, 2)             \
    _(filter_differentiate_state_fields, 1)        \
    _(filter_fir_config_fields, 2)                 \
    _(filter_fir_state_fields, 3)                  \
    _(filter_integrate_state_fields, 1)            \
    _(freq_fft_config_fields, 1)                   \
    _(freq_ifft_config_fields, 1)                  \
    _(freq_multiply_config_fields, 1)              \
    _(freq_overlap_add_config_fields, 2)           \
    _(freq_overlap_add_state_fields, 1)            \
    _(freq_overlap_save_config_fields, 2)          \
    _(freq_overlap_save_state_fields, 2)           \
    _(freq_window_config_fields, 2)                \
    _(freq_shift_config_fields, 1)                 \
    _(freq_shift_state_fields, 5)                  \
    _(generation_dc_config_fields, 1)              \
    _(generation_envelope_config_fields, 5)        \
    _(generation_envelope_state_fields, 2)         \
    _(generation_impulse_config_fields, 2)         \
    _(generation_impulse_state_fields, 1)          \
    _(generation_lfo_config_fields, 4)             \
    _(generation_lfo_state_fields, 1)              \
    _(generation_noise_config_fields, 2)           \
    _(generation_noise_state_fields, 2)            \
    _(generation_oscillator_config_fields, 4)      \
    _(generation_oscillator_state_fields, 1)       \
    _(interpolation_lagrange_config_fields, 1)     \
    _(interpolation_lagrange_state_fields, 2)      \
    _(interpolation_sinc_config_fields, 1)         \
    _(interpolation_sinc_state_fields, 1)          \
    _(mix_crossfade_config_fields, 1)              \
    _(mix_matrix_config_fields, 2)                 \
    _(mix_pan_stereo_config_fields, 1)             \
    _(mix_wet_dry_config_fields, 1)                \
    _(modulation_amplitude_config_fields, 1)       \
    _(modulation_frequency_config_fields, 1)       \
    _(modulation_frequency_state_fields, 4)        \
    _(modulation_phase_config_fields, 1)           \
    _(modulation_phase_state_fields, 3)            \
    _(modulation_scrub_config_fields, 1)           \
    _(nonlinear_bitcrush_config_fields, 1)         \
    _(nonlinear_sample_hold_config_fields, 1)      \
    _(nonlinear_sample_hold_state_fields, 2)       \
    _(nonlinear_waveshape_config_fields, 2)        \
    _(src_antialias_config_fields, 2)              \
    _(src_antialias_state_fields, 2)               \
    _(src_antiimage_config_fields, 2)              \
    _(src_antiimage_state_fields, 2)               \
    _(src_convert_format_config_fields, 2)         \
    _(src_downsample_config_fields, 1)             \
    _(src_upsample_config_fields, 1)

#define APG_DECLARE_ATOM_FIELD_TABLE(name, count)        APG_DECLARE_ATOM_FIELD_TABLE_EXPAND(name, count)
#define APG_DECLARE_ATOM_FIELD_TABLE_EXPAND(name, count) APG_DECLARE_ATOM_FIELD_TABLE_##count(name)
#define APG_DECLARE_ATOM_FIELD_TABLE_0(name)
#define APG_DECLARE_ATOM_FIELD_TABLE_1(name) extern const atom_field_desc_t name[1];
#define APG_DECLARE_ATOM_FIELD_TABLE_2(name) extern const atom_field_desc_t name[2];
#define APG_DECLARE_ATOM_FIELD_TABLE_3(name) extern const atom_field_desc_t name[3];
#define APG_DECLARE_ATOM_FIELD_TABLE_4(name) extern const atom_field_desc_t name[4];
#define APG_DECLARE_ATOM_FIELD_TABLE_5(name) extern const atom_field_desc_t name[5];

APG_ATOM_FIELD_TABLES(APG_DECLARE_ATOM_FIELD_TABLE)

#undef APG_DECLARE_ATOM_FIELD_TABLE
#undef APG_DECLARE_ATOM_FIELD_TABLE_EXPAND
#undef APG_DECLARE_ATOM_FIELD_TABLE_0
#undef APG_DECLARE_ATOM_FIELD_TABLE_1
#undef APG_DECLARE_ATOM_FIELD_TABLE_2
#undef APG_DECLARE_ATOM_FIELD_TABLE_3
#undef APG_DECLARE_ATOM_FIELD_TABLE_4
#undef APG_DECLARE_ATOM_FIELD_TABLE_5

#endif // ATOM_FIELD_DESCRIPTORS_H
