#include "atom/atom_field_descriptors.h"
#include "atom/dsp_atoms.h"
#include <atom_registry.h>
#include <stddef.h>
#include <string.h>

// ─────────────────────────────────────────────
// Thunk generation
// ─────────────────────────────────────────────

#define THUNK(_)                                                                                                   \
    static void _##_thunk(atom_call_t *call) {                                                                     \
        _((_##_out_t *)call->out, (_##_in_t *)call->in, (_##_params_t *)call->config, (_##_state_t *)call->state); \
    }

#define REGISTER_ALL(_) \
    _(freq_fft)         \
    _(freq_ifft)        \
    _(freq_multiply) /* end */

void amplitude_clip_soft_thunk(atom_call_t *call) {
    amplitude_clip_soft_process(
        (amplitude_clip_soft_out_t *)call->out, (amplitude_clip_soft_in_t *)call->in,
        (amplitude_clip_soft_params_t *)call->config, (amplitude_clip_soft_state_t *)call->state, call->info
    );
}

void amplitude_multiply_thunk(atom_call_t *call) {
    amplitude_multiply_process(
        (amplitude_multiply_out_t *)call->out, (amplitude_multiply_in_t *)call->in,
        (amplitude_multiply_params_t *)call->config, (amplitude_multiply_state_t *)call->state, call->info
    );
}

void delay_line_thunk(atom_call_t *call) {
    delay_line_process(
        (delay_line_out_t *)call->out, (delay_line_in_t *)call->in, (delay_line_params_t *)call->config,
        (delay_line_state_t *)call->state, call->info
    );
}

void filter_biquad_thunk(atom_call_t *call) {
    filter_biquad_process(
        (filter_biquad_out_t *)call->out, (filter_biquad_in_t *)call->in, (filter_biquad_params_t *)call->config,
        (filter_biquad_state_t *)call->state, call->info
    );
}

void generation_dc_thunk(atom_call_t *call) {
    generation_dc_process(
        (generation_dc_out_t *)call->out, (generation_dc_in_t *)call->in, (generation_dc_params_t *)call->config,
        (generation_dc_state_t *)call->state, call->info
    );
}

void generation_lfo_thunk(atom_call_t *call) {
    generation_lfo_process(
        (generation_lfo_out_t *)call->out, (generation_lfo_in_t *)call->in, (generation_lfo_params_t *)call->config,
        (generation_lfo_state_t *)call->state, call->info
    );
}

void mix_wet_dry_thunk(atom_call_t *call) {
    mix_wet_dry_process(
        (mix_wet_dry_out_t *)call->out, (mix_wet_dry_in_t *)call->in, (mix_wet_dry_params_t *)call->config,
        (mix_wet_dry_state_t *)call->state, call->info
    );
}

void modulation_amplitude_thunk(atom_call_t *call) {
    modulation_amplitude_process(
        (modulation_amplitude_out_t *)call->out, (modulation_amplitude_in_t *)call->in,
        (modulation_amplitude_params_t *)call->config, (modulation_amplitude_state_t *)call->state, call->info
    );
}

void amplitude_smooth_thunk(atom_call_t *call) {
    amplitude_smooth_process(
        (amplitude_smooth_out_t *)call->out, (amplitude_smooth_in_t *)call->in,
        (amplitude_smooth_params_t *)call->config, (amplitude_smooth_state_t *)call->state, call->info
    );
}

void detect_envelope_thunk(atom_call_t *call) {
    detect_envelope_process(
        (detect_envelope_out_t *)call->out, (detect_envelope_in_t *)call->in, (detect_envelope_params_t *)call->config,
        (detect_envelope_state_t *)call->state, call->info
    );
}

void detect_peak_thunk(atom_call_t *call) {
    detect_peak_process(
        (detect_peak_out_t *)call->out, (detect_peak_in_t *)call->in, (detect_peak_params_t *)call->config,
        (detect_peak_state_t *)call->state, call->info
    );
}

void detect_threshold_thunk(atom_call_t *call) {
    detect_threshold_process(
        (detect_threshold_out_t *)call->out, (detect_threshold_in_t *)call->in,
        (detect_threshold_params_t *)call->config, (detect_threshold_state_t *)call->state, call->info
    );
}

void delay_unit_thunk(atom_call_t *call) {
    delay_unit_process(
        (delay_unit_out_t *)call->out, (delay_unit_in_t *)call->in, (delay_unit_params_t *)call->config,
        (delay_unit_state_t *)call->state, call->info
    );
}

void delay_fractional_thunk(atom_call_t *call) {
    delay_fractional_process(
        (delay_fractional_out_t *)call->out, (delay_fractional_in_t *)call->in,
        (delay_fractional_params_t *)call->config, (delay_fractional_state_t *)call->state, call->info
    );
}

void delay_tap_feedback_thunk(atom_call_t *call) {
    delay_tap_feedback_process(
        (delay_tap_feedback_out_t *)call->out, (delay_tap_feedback_in_t *)call->in,
        (delay_tap_feedback_params_t *)call->config, (delay_tap_feedback_state_t *)call->state, call->info
    );
}

void delay_tap_feedforward_thunk(atom_call_t *call) {
    delay_tap_feedforward_process(
        (delay_tap_feedforward_out_t *)call->out, (delay_tap_feedforward_in_t *)call->in,
        (delay_tap_feedforward_params_t *)call->config, (delay_tap_feedforward_state_t *)call->state, call->info
    );
}

void amplitude_add_thunk(atom_call_t *call) {
    amplitude_add_process(
        (amplitude_add_out_t *)call->out, (amplitude_add_in_t *)call->in, (amplitude_add_params_t *)call->config,
        (amplitude_add_state_t *)call->state, call->info
    );
}

void amplitude_subtract_thunk(atom_call_t *call) {
    amplitude_subtract_process(
        (amplitude_subtract_out_t *)call->out, (amplitude_subtract_in_t *)call->in,
        (amplitude_subtract_params_t *)call->config, (amplitude_subtract_state_t *)call->state, call->info
    );
}

void amplitude_divide_thunk(atom_call_t *call) {
    amplitude_divide_process(
        (amplitude_divide_out_t *)call->out, (amplitude_divide_in_t *)call->in,
        (amplitude_divide_params_t *)call->config, (amplitude_divide_state_t *)call->state, call->info
    );
}

void mix_crossfade_thunk(atom_call_t *call) {
    mix_crossfade_process(
        (mix_crossfade_out_t *)call->out, (mix_crossfade_in_t *)call->in, (mix_crossfade_params_t *)call->config,
        (mix_crossfade_state_t *)call->state, call->info
    );
}

void amplitude_clip_hard_thunk(atom_call_t *call) {
    amplitude_clip_hard_process(
        (amplitude_clip_hard_out_t *)call->out, (amplitude_clip_hard_in_t *)call->in,
        (amplitude_clip_hard_params_t *)call->config, (amplitude_clip_hard_state_t *)call->state, call->info
    );
}

void amplitude_accumulate_thunk(atom_call_t *call) {
    amplitude_accumulate_process(
        (amplitude_accumulate_out_t *)call->out, (amplitude_accumulate_in_t *)call->in,
        (amplitude_accumulate_params_t *)call->config, (amplitude_accumulate_state_t *)call->state, call->info
    );
}

void amplitude_latch_thunk(atom_call_t *call) {
    amplitude_latch_process(
        (amplitude_latch_out_t *)call->out, (amplitude_latch_in_t *)call->in, (amplitude_latch_params_t *)call->config,
        (amplitude_latch_state_t *)call->state, call->info
    );
}

void amplitude_normalize_thunk(atom_call_t *call) {
    amplitude_normalize_process(
        (amplitude_normalize_out_t *)call->out, (amplitude_normalize_in_t *)call->in,
        (amplitude_normalize_params_t *)call->config, (amplitude_normalize_state_t *)call->state, call->info
    );
}

void freq_window_thunk(atom_call_t *call) {
    freq_window_process(
        (freq_window_out_t *)call->out, (freq_window_in_t *)call->in, (freq_window_params_t *)call->config,
        (freq_window_state_t *)call->state, call->info
    );
}

void freq_quantize_thunk(atom_call_t *call) {
    freq_quantize_process(
        (freq_quantize_out_t *)call->out, (freq_quantize_in_t *)call->in, (freq_quantize_params_t *)call->config,
        (freq_quantize_state_t *)call->state, call->info
    );
}

void freq_shift_thunk(atom_call_t *call) {
    freq_shift_process(
        (freq_shift_out_t *)call->out, (freq_shift_in_t *)call->in, (freq_shift_params_t *)call->config,
        (freq_shift_state_t *)call->state, call->info
    );
}

void freq_overlap_add_thunk(atom_call_t *call) {
    freq_overlap_add_process(
        (freq_overlap_add_out_t *)call->out, (freq_overlap_add_in_t *)call->in,
        (freq_overlap_add_params_t *)call->config, (freq_overlap_add_state_t *)call->state, call->info
    );
}

void freq_overlap_save_thunk(atom_call_t *call) {
    freq_overlap_save_process(
        (freq_overlap_save_out_t *)call->out, (freq_overlap_save_in_t *)call->in,
        (freq_overlap_save_params_t *)call->config, (freq_overlap_save_state_t *)call->state, call->info
    );
}

void interpolation_linear_thunk(atom_call_t *call) {
    interpolation_linear_process(
        (interpolation_linear_out_t *)call->out, (interpolation_linear_in_t *)call->in,
        (interpolation_linear_params_t *)call->config, (interpolation_linear_state_t *)call->state, call->info
    );
}

void interpolation_cubic_thunk(atom_call_t *call) {
    interpolation_cubic_process(
        (interpolation_cubic_out_t *)call->out, (interpolation_cubic_in_t *)call->in,
        (interpolation_cubic_params_t *)call->config, (interpolation_cubic_state_t *)call->state, call->info
    );
}

void interpolation_lagrange_thunk(atom_call_t *call) {
    interpolation_lagrange_process(
        (interpolation_lagrange_out_t *)call->out, (interpolation_lagrange_in_t *)call->in,
        (interpolation_lagrange_params_t *)call->config, (interpolation_lagrange_state_t *)call->state, call->info
    );
}

void interpolation_sinc_thunk(atom_call_t *call) {
    interpolation_sinc_process(
        (interpolation_sinc_out_t *)call->out, (interpolation_sinc_in_t *)call->in,
        (interpolation_sinc_params_t *)call->config, (interpolation_sinc_state_t *)call->state, call->info
    );
}

void src_convert_format_thunk(atom_call_t *call) {
    src_convert_format_process(
        (src_convert_format_out_t *)call->out, (src_convert_format_in_t *)call->in,
        (src_convert_format_params_t *)call->config, (src_convert_format_state_t *)call->state, call->info
    );
}

void src_downsample_thunk(atom_call_t *call) {
    src_downsample_process(
        (src_downsample_out_t *)call->out, (src_downsample_in_t *)call->in, (src_downsample_params_t *)call->config,
        (src_downsample_state_t *)call->state, call->info
    );
}

void src_upsample_thunk(atom_call_t *call) {
    src_upsample_process(
        (src_upsample_out_t *)call->out, (src_upsample_in_t *)call->in, (src_upsample_params_t *)call->config,
        (src_upsample_state_t *)call->state, call->info
    );
}

void src_antialias_thunk(atom_call_t *call) {
    src_antialias_process(
        (src_antialias_out_t *)call->out, (src_antialias_in_t *)call->in, (src_antialias_params_t *)call->config,
        (src_antialias_state_t *)call->state, call->info
    );
}

void src_antiimage_thunk(atom_call_t *call) {
    src_antiimage_process(
        (src_antiimage_out_t *)call->out, (src_antiimage_in_t *)call->in, (src_antiimage_params_t *)call->config,
        (src_antiimage_state_t *)call->state, call->info
    );
}

void generation_impulse_thunk(atom_call_t *call) {
    generation_impulse_process(
        (generation_impulse_out_t *)call->out, (generation_impulse_in_t *)call->in,
        (generation_impulse_params_t *)call->config, (generation_impulse_state_t *)call->state, call->info
    );
}

void generation_noise_thunk(atom_call_t *call) {
    generation_noise_process(
        (generation_noise_out_t *)call->out, (generation_noise_in_t *)call->in,
        (generation_noise_params_t *)call->config, (generation_noise_state_t *)call->state, call->info
    );
}

void generation_envelope_thunk(atom_call_t *call) {
    generation_envelope_process(
        (generation_envelope_out_t *)call->out, (generation_envelope_in_t *)call->in,
        (generation_envelope_params_t *)call->config, (generation_envelope_state_t *)call->state, call->info
    );
}

void generation_oscillator_thunk(atom_call_t *call) {
    generation_oscillator_process(
        (generation_oscillator_out_t *)call->out, (generation_oscillator_in_t *)call->in,
        (generation_oscillator_params_t *)call->config, (generation_oscillator_state_t *)call->state, call->info
    );
}

void modulation_ring_thunk(atom_call_t *call) {
    modulation_ring_process(
        (modulation_ring_out_t *)call->out, (modulation_ring_in_t *)call->in, (modulation_ring_params_t *)call->config,
        (modulation_ring_state_t *)call->state, call->info
    );
}

void modulation_frequency_thunk(atom_call_t *call) {
    modulation_frequency_process(
        (modulation_frequency_out_t *)call->out, (modulation_frequency_in_t *)call->in,
        (modulation_frequency_params_t *)call->config, (modulation_frequency_state_t *)call->state, call->info
    );
}

void modulation_phase_thunk(atom_call_t *call) {
    modulation_phase_process(
        (modulation_phase_out_t *)call->out, (modulation_phase_in_t *)call->in,
        (modulation_phase_params_t *)call->config, (modulation_phase_state_t *)call->state, call->info
    );
}

void modulation_scrub_thunk(atom_call_t *call) {
    modulation_scrub_process(
        (modulation_scrub_out_t *)call->out, (modulation_scrub_in_t *)call->in,
        (modulation_scrub_params_t *)call->config, (modulation_scrub_state_t *)call->state, call->info
    );
}

void detect_slope_thunk(atom_call_t *call) {
    detect_slope_process(
        (detect_slope_out_t *)call->out, (detect_slope_in_t *)call->in, (detect_slope_params_t *)call->config,
        (detect_slope_state_t *)call->state, call->info
    );
}

void detect_rms_thunk(atom_call_t *call) {
    detect_rms_process(
        (detect_rms_out_t *)call->out, (detect_rms_in_t *)call->in, (detect_rms_params_t *)call->config,
        (detect_rms_state_t *)call->state, call->info
    );
}

void detect_zero_crossing_thunk(atom_call_t *call) {
    detect_zero_crossing_process(
        (detect_zero_crossing_out_t *)call->out, (detect_zero_crossing_in_t *)call->in,
        (detect_zero_crossing_params_t *)call->config, (detect_zero_crossing_state_t *)call->state, call->info
    );
}

void detect_autocorrelate_thunk(atom_call_t *call) {
    detect_autocorrelate_process(
        (detect_autocorrelate_out_t *)call->out, (detect_autocorrelate_in_t *)call->in,
        (detect_autocorrelate_params_t *)call->config, (detect_autocorrelate_state_t *)call->state, call->info
    );
}

void detect_pitch_thunk(atom_call_t *call) {
    detect_pitch_process(
        (detect_pitch_out_t *)call->out, (detect_pitch_in_t *)call->in, (detect_pitch_params_t *)call->config,
        (detect_pitch_state_t *)call->state, call->info
    );
}

void filter_allpass_thunk(atom_call_t *call) {
    filter_allpass_process(
        (filter_allpass_out_t *)call->out, (filter_allpass_in_t *)call->in, (filter_allpass_params_t *)call->config,
        (filter_allpass_state_t *)call->state, call->info
    );
}

void filter_comb_ff_thunk(atom_call_t *call) {
    filter_comb_ff_process(
        (filter_comb_ff_out_t *)call->out, (filter_comb_ff_in_t *)call->in, (filter_comb_ff_params_t *)call->config,
        (filter_comb_ff_state_t *)call->state, call->info
    );
}

void filter_comb_fb_thunk(atom_call_t *call) {
    filter_comb_fb_process(
        (filter_comb_fb_out_t *)call->out, (filter_comb_fb_in_t *)call->in, (filter_comb_fb_params_t *)call->config,
        (filter_comb_fb_state_t *)call->state, call->info
    );
}

void filter_dc_block_thunk(atom_call_t *call) {
    filter_dc_block_process(
        (filter_dc_block_out_t *)call->out, (filter_dc_block_in_t *)call->in, (filter_dc_block_params_t *)call->config,
        (filter_dc_block_state_t *)call->state, call->info
    );
}

void filter_differentiate_thunk(atom_call_t *call) {
    filter_differentiate_process(
        (filter_differentiate_out_t *)call->out, (filter_differentiate_in_t *)call->in,
        (filter_differentiate_params_t *)call->config, (filter_differentiate_state_t *)call->state, call->info
    );
}

void filter_integrate_thunk(atom_call_t *call) {
    filter_integrate_process(
        (filter_integrate_out_t *)call->out, (filter_integrate_in_t *)call->in,
        (filter_integrate_params_t *)call->config, (filter_integrate_state_t *)call->state, call->info
    );
}

void filter_fir_thunk(atom_call_t *call) {
    filter_fir_process(
        (filter_fir_out_t *)call->out, (filter_fir_in_t *)call->in, (filter_fir_params_t *)call->config,
        (filter_fir_state_t *)call->state, call->info
    );
}

void mix_matrix_thunk(atom_call_t *call) {
    mix_matrix_process(
        (mix_matrix_out_t *)call->out, (mix_matrix_in_t *)call->in, (mix_matrix_params_t *)call->config,
        (mix_matrix_state_t *)call->state, call->info
    );
}

void mix_pan_stereo_thunk(atom_call_t *call) {
    mix_pan_stereo_process(
        (mix_pan_stereo_out_t *)call->out, (mix_pan_stereo_in_t *)call->in, (mix_pan_stereo_params_t *)call->config,
        (mix_pan_stereo_state_t *)call->state, call->info
    );
}

void mix_encode_ms_thunk(atom_call_t *call) {
    mix_encode_ms_process(
        (mix_encode_ms_out_t *)call->out, (mix_encode_ms_in_t *)call->in, (mix_encode_ms_params_t *)call->config,
        (mix_encode_ms_state_t *)call->state, call->info
    );
}

void mix_decode_ms_thunk(atom_call_t *call) {
    mix_decode_ms_process(
        (mix_decode_ms_out_t *)call->out, (mix_decode_ms_in_t *)call->in, (mix_decode_ms_params_t *)call->config,
        (mix_decode_ms_state_t *)call->state, call->info
    );
}

void nonlinear_bitcrush_thunk(atom_call_t *call) {
    nonlinear_bitcrush_process(
        (nonlinear_bitcrush_out_t *)call->out, (nonlinear_bitcrush_in_t *)call->in,
        (nonlinear_bitcrush_params_t *)call->config, (nonlinear_bitcrush_state_t *)call->state, call->info
    );
}

void nonlinear_waveshape_thunk(atom_call_t *call) {
    nonlinear_waveshape_process(
        (nonlinear_waveshape_out_t *)call->out, (nonlinear_waveshape_in_t *)call->in,
        (nonlinear_waveshape_params_t *)call->config, (nonlinear_waveshape_state_t *)call->state, call->info
    );
}

void nonlinear_samplerate_reduce_thunk(atom_call_t *call) {
    nonlinear_samplerate_reduce_process(
        (nonlinear_samplerate_reduce_out_t *)call->out, (nonlinear_samplerate_reduce_in_t *)call->in,
        (nonlinear_samplerate_reduce_params_t *)call->config, (nonlinear_samplerate_reduce_state_t *)call->state,
        call->info
    );
}

REGISTER_ALL(THUNK)

// ─────────────────────────────────────────────
// Registry table
// ─────────────────────────────────────────────

#define REGISTRY_FIELDS(fields) fields, sizeof(fields) / sizeof((fields)[0])
#define REGISTRY_NO_FIELDS      NULL, 0

#define REGISTRY_ATOM(atom_name, config_fields, state_fields) \
    REGISTRY_ATOM_EXPAND(atom_name, config_fields, state_fields)

#define REGISTRY_ATOM_EXPAND(atom_name, c_fields, n_cfg, s_fields, n_st)                                \
    {                                                                                                   \
        .name = #atom_name, .thunk = atom_name##_thunk, .out_size = sizeof(atom_name##_out_t),          \
        .in_size = sizeof(atom_name##_in_t), .config_size = sizeof(atom_name##_params_t),               \
        .state_size = sizeof(atom_name##_state_t), .config_fields = c_fields, .n_config_fields = n_cfg, \
        .state_fields = s_fields, .n_state_fields = n_st,                                               \
    }

static atom_registry_entry_t g_registry[] = {
    REGISTRY_ATOM(amplitude_accumulate, REGISTRY_NO_FIELDS, REGISTRY_FIELDS(amplitude_accumulate_state_fields)),
    REGISTRY_ATOM(
        amplitude_latch, REGISTRY_FIELDS(amplitude_latch_config_fields), REGISTRY_FIELDS(amplitude_latch_state_fields)
    ),
    REGISTRY_ATOM(amplitude_add, REGISTRY_NO_FIELDS, REGISTRY_NO_FIELDS),
    REGISTRY_ATOM(amplitude_clip_hard, REGISTRY_FIELDS(amplitude_clip_hard_config_fields), REGISTRY_NO_FIELDS),
    REGISTRY_ATOM(amplitude_clip_soft, REGISTRY_FIELDS(amplitude_clip_soft_config_fields), REGISTRY_NO_FIELDS),
    REGISTRY_ATOM(amplitude_divide, REGISTRY_FIELDS(amplitude_divide_config_fields), REGISTRY_NO_FIELDS),
    REGISTRY_ATOM(amplitude_multiply, REGISTRY_NO_FIELDS, REGISTRY_NO_FIELDS),
    REGISTRY_ATOM(
        amplitude_normalize,
        REGISTRY_FIELDS(amplitude_normalize_config_fields),
        REGISTRY_FIELDS(amplitude_normalize_state_fields)
    ),
    REGISTRY_ATOM(
        amplitude_smooth,
        REGISTRY_FIELDS(amplitude_smooth_config_fields),
        REGISTRY_FIELDS(amplitude_smooth_state_fields)
    ),
    REGISTRY_ATOM(amplitude_subtract, REGISTRY_NO_FIELDS, REGISTRY_NO_FIELDS),
    REGISTRY_ATOM(
        delay_fractional,
        REGISTRY_FIELDS(delay_fractional_config_fields),
        REGISTRY_FIELDS(delay_fractional_state_fields)
    ),
    REGISTRY_ATOM(delay_line, REGISTRY_FIELDS(delay_line_config_fields), REGISTRY_FIELDS(delay_line_state_fields)),
    REGISTRY_ATOM(delay_tap_feedback, REGISTRY_FIELDS(delay_tap_feedback_config_fields), REGISTRY_NO_FIELDS),
    REGISTRY_ATOM(delay_tap_feedforward, REGISTRY_FIELDS(delay_tap_feedforward_config_fields), REGISTRY_NO_FIELDS),
    REGISTRY_ATOM(delay_unit, REGISTRY_NO_FIELDS, REGISTRY_FIELDS(delay_unit_state_fields)),
    REGISTRY_ATOM(
        detect_autocorrelate,
        REGISTRY_FIELDS(detect_autocorrelate_config_fields),
        REGISTRY_FIELDS(detect_autocorrelate_state_fields)
    ),
    REGISTRY_ATOM(
        detect_pitch, REGISTRY_FIELDS(detect_pitch_config_fields), REGISTRY_FIELDS(detect_pitch_state_fields)
    ),
    REGISTRY_ATOM(
        detect_envelope, REGISTRY_FIELDS(detect_envelope_config_fields), REGISTRY_FIELDS(detect_envelope_state_fields)
    ),
    REGISTRY_ATOM(detect_peak, REGISTRY_FIELDS(detect_peak_config_fields), REGISTRY_FIELDS(detect_peak_state_fields)),
    REGISTRY_ATOM(detect_rms, REGISTRY_FIELDS(detect_rms_config_fields), REGISTRY_FIELDS(detect_rms_state_fields)),
    REGISTRY_ATOM(detect_slope, REGISTRY_NO_FIELDS, REGISTRY_FIELDS(detect_slope_state_fields)),
    REGISTRY_ATOM(detect_threshold, REGISTRY_FIELDS(detect_threshold_config_fields), REGISTRY_NO_FIELDS),
    REGISTRY_ATOM(detect_zero_crossing, REGISTRY_NO_FIELDS, REGISTRY_FIELDS(detect_zero_crossing_state_fields)),
    REGISTRY_ATOM(
        filter_allpass, REGISTRY_FIELDS(filter_allpass_config_fields), REGISTRY_FIELDS(filter_allpass_state_fields)
    ),
    REGISTRY_ATOM(
        filter_biquad, REGISTRY_FIELDS(filter_biquad_config_fields), REGISTRY_FIELDS(filter_biquad_state_fields)
    ),
    REGISTRY_ATOM(
        filter_comb_fb, REGISTRY_FIELDS(filter_comb_fb_config_fields), REGISTRY_FIELDS(filter_comb_fb_state_fields)
    ),
    REGISTRY_ATOM(
        filter_comb_ff, REGISTRY_FIELDS(filter_comb_ff_config_fields), REGISTRY_FIELDS(filter_comb_ff_state_fields)
    ),
    REGISTRY_ATOM(
        filter_dc_block, REGISTRY_FIELDS(filter_dc_block_config_fields), REGISTRY_FIELDS(filter_dc_block_state_fields)
    ),
    REGISTRY_ATOM(filter_differentiate, REGISTRY_NO_FIELDS, REGISTRY_FIELDS(filter_differentiate_state_fields)),
    REGISTRY_ATOM(filter_fir, REGISTRY_FIELDS(filter_fir_config_fields), REGISTRY_FIELDS(filter_fir_state_fields)),
    REGISTRY_ATOM(filter_integrate, REGISTRY_NO_FIELDS, REGISTRY_FIELDS(filter_integrate_state_fields)),
    REGISTRY_ATOM(freq_fft, REGISTRY_FIELDS(freq_fft_config_fields), REGISTRY_NO_FIELDS),
    REGISTRY_ATOM(freq_ifft, REGISTRY_FIELDS(freq_ifft_config_fields), REGISTRY_NO_FIELDS),
    REGISTRY_ATOM(freq_multiply, REGISTRY_FIELDS(freq_multiply_config_fields), REGISTRY_NO_FIELDS),
    REGISTRY_ATOM(
        freq_overlap_add,
        REGISTRY_FIELDS(freq_overlap_add_config_fields),
        REGISTRY_FIELDS(freq_overlap_add_state_fields)
    ),
    REGISTRY_ATOM(
        freq_overlap_save,
        REGISTRY_FIELDS(freq_overlap_save_config_fields),
        REGISTRY_FIELDS(freq_overlap_save_state_fields)
    ),
    REGISTRY_ATOM(
        freq_overlap_add,
        REGISTRY_FIELDS(freq_overlap_add_config_fields),
        REGISTRY_FIELDS(freq_overlap_add_state_fields)
    ),
    REGISTRY_ATOM(
        freq_overlap_save,
        REGISTRY_FIELDS(freq_overlap_save_config_fields),
        REGISTRY_FIELDS(freq_overlap_save_state_fields)
    ),
    REGISTRY_ATOM(freq_window, REGISTRY_FIELDS(freq_window_config_fields), REGISTRY_NO_FIELDS),
    REGISTRY_ATOM(freq_shift, REGISTRY_FIELDS(freq_shift_config_fields), REGISTRY_FIELDS(freq_shift_state_fields)),
    REGISTRY_ATOM(generation_dc, REGISTRY_FIELDS(generation_dc_config_fields), REGISTRY_NO_FIELDS),
    REGISTRY_ATOM(
        generation_envelope,
        REGISTRY_FIELDS(generation_envelope_config_fields),
        REGISTRY_FIELDS(generation_envelope_state_fields)
    ),
    REGISTRY_ATOM(
        generation_impulse,
        REGISTRY_FIELDS(generation_impulse_config_fields),
        REGISTRY_FIELDS(generation_impulse_state_fields)
    ),
    REGISTRY_ATOM(
        generation_lfo, REGISTRY_FIELDS(generation_lfo_config_fields), REGISTRY_FIELDS(generation_lfo_state_fields)
    ),
    REGISTRY_ATOM(
        generation_noise,
        REGISTRY_FIELDS(generation_noise_config_fields),
        REGISTRY_FIELDS(generation_noise_state_fields)
    ),
    REGISTRY_ATOM(
        generation_oscillator,
        REGISTRY_FIELDS(generation_oscillator_config_fields),
        REGISTRY_FIELDS(generation_oscillator_state_fields)
    ),
    REGISTRY_ATOM(interpolation_cubic, REGISTRY_NO_FIELDS, REGISTRY_NO_FIELDS),
    REGISTRY_ATOM(
        interpolation_lagrange,
        REGISTRY_FIELDS(interpolation_lagrange_config_fields),
        REGISTRY_FIELDS(interpolation_lagrange_state_fields)
    ),
    REGISTRY_ATOM(interpolation_linear, REGISTRY_NO_FIELDS, REGISTRY_NO_FIELDS),
    REGISTRY_ATOM(
        interpolation_sinc,
        REGISTRY_FIELDS(interpolation_sinc_config_fields),
        REGISTRY_FIELDS(interpolation_sinc_state_fields)
    ),
    REGISTRY_ATOM(mix_crossfade, REGISTRY_FIELDS(mix_crossfade_config_fields), REGISTRY_NO_FIELDS),
    REGISTRY_ATOM(mix_decode_ms, REGISTRY_NO_FIELDS, REGISTRY_NO_FIELDS),
    REGISTRY_ATOM(mix_encode_ms, REGISTRY_NO_FIELDS, REGISTRY_NO_FIELDS),
    REGISTRY_ATOM(mix_matrix, REGISTRY_FIELDS(mix_matrix_config_fields), REGISTRY_NO_FIELDS),
    REGISTRY_ATOM(mix_pan_stereo, REGISTRY_FIELDS(mix_pan_stereo_config_fields), REGISTRY_NO_FIELDS),
    REGISTRY_ATOM(mix_wet_dry, REGISTRY_FIELDS(mix_wet_dry_config_fields), REGISTRY_NO_FIELDS),
    REGISTRY_ATOM(modulation_amplitude, REGISTRY_FIELDS(modulation_amplitude_config_fields), REGISTRY_NO_FIELDS),
    REGISTRY_ATOM(
        modulation_frequency,
        REGISTRY_FIELDS(modulation_frequency_config_fields),
        REGISTRY_FIELDS(modulation_frequency_state_fields)
    ),
    REGISTRY_ATOM(
        modulation_phase,
        REGISTRY_FIELDS(modulation_phase_config_fields),
        REGISTRY_FIELDS(modulation_phase_state_fields)
    ),
    REGISTRY_ATOM(modulation_ring, REGISTRY_NO_FIELDS, REGISTRY_NO_FIELDS),
    REGISTRY_ATOM(modulation_scrub, REGISTRY_FIELDS(modulation_scrub_config_fields), REGISTRY_NO_FIELDS),
    REGISTRY_ATOM(nonlinear_bitcrush, REGISTRY_FIELDS(nonlinear_bitcrush_config_fields), REGISTRY_NO_FIELDS),
    REGISTRY_ATOM(
        nonlinear_samplerate_reduce,
        REGISTRY_FIELDS(nonlinear_samplerate_reduce_config_fields),
        REGISTRY_FIELDS(nonlinear_samplerate_reduce_state_fields)
    ),
    REGISTRY_ATOM(nonlinear_waveshape, REGISTRY_FIELDS(nonlinear_waveshape_config_fields), REGISTRY_NO_FIELDS),
    REGISTRY_ATOM(
        src_antialias, REGISTRY_FIELDS(src_antialias_config_fields), REGISTRY_FIELDS(src_antialias_state_fields)
    ),
    REGISTRY_ATOM(
        src_antiimage, REGISTRY_FIELDS(src_antiimage_config_fields), REGISTRY_FIELDS(src_antiimage_state_fields)
    ),
    REGISTRY_ATOM(src_convert_format, REGISTRY_FIELDS(src_convert_format_config_fields), REGISTRY_NO_FIELDS),
    REGISTRY_ATOM(src_downsample, REGISTRY_FIELDS(src_downsample_config_fields), REGISTRY_NO_FIELDS),
    REGISTRY_ATOM(src_upsample, REGISTRY_FIELDS(src_upsample_config_fields), REGISTRY_NO_FIELDS),
    REGISTRY_ATOM(freq_quantize, REGISTRY_NO_FIELDS, REGISTRY_NO_FIELDS),
};

static const int g_registry_count = sizeof(g_registry) / sizeof(g_registry[0]);

void atom_registry_init(void) {}

static const atom_field_desc_t *registry_in_fields_for_name(const char *name, size_t *out_len) {
    if (out_len)
        *out_len = 0u;
    if (!name)
        return NULL;
    if (strcmp(name, "delay_tap_feedback") == 0) {
        if (out_len)
            *out_len = sizeof(delay_tap_feedback_in_fields) / sizeof(delay_tap_feedback_in_fields[0]);
        return delay_tap_feedback_in_fields;
    }
    if (strcmp(name, "delay_tap_feedforward") == 0) {
        if (out_len)
            *out_len = sizeof(delay_tap_feedforward_in_fields) / sizeof(delay_tap_feedforward_in_fields[0]);
        return delay_tap_feedforward_in_fields;
    }
    return NULL;
}

const atom_field_desc_t *atom_registry_in_fields(const atom_registry_entry_t *atom, size_t *out_len) {
    if (!atom)
        return NULL;
    return registry_in_fields_for_name(atom->name, out_len);
}

const atom_registry_entry_t *atom_registry_find(const char *name) {
    for (int i = 0; i < g_registry_count; i++) {
        if (strcmp(g_registry[i].name, name) == 0)
            return &g_registry[i];
    }
    return NULL;
}

int atom_registry_count(void) { return g_registry_count; }

const atom_registry_entry_t *atom_registry_get(int index) {
    if (index < 0 || index >= g_registry_count)
        return NULL;
    return &g_registry[index];
}
