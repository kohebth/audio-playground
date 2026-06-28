#include "atom/dsp_atoms.h"
#include <atom_registry.h>
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

static void amplitude_clip_soft_thunk(atom_call_t *call) {
    amplitude_clip_soft_process(
        (amplitude_clip_soft_out_t *)call->out, (amplitude_clip_soft_in_t *)call->in,
        (amplitude_clip_soft_params_t *)call->config, (amplitude_clip_soft_state_t *)call->state, call->info
    );
}

static void amplitude_multiply_thunk(atom_call_t *call) {
    amplitude_multiply_process(
        (amplitude_multiply_out_t *)call->out, (amplitude_multiply_in_t *)call->in,
        (amplitude_multiply_params_t *)call->config, (amplitude_multiply_state_t *)call->state, call->info
    );
}

static void delay_line_thunk(atom_call_t *call) {
    delay_line_process(
        (delay_line_out_t *)call->out, (delay_line_in_t *)call->in, (delay_line_params_t *)call->config,
        (delay_line_state_t *)call->state, call->info
    );
}

static void filter_biquad_thunk(atom_call_t *call) {
    filter_biquad_process(
        (filter_biquad_out_t *)call->out, (filter_biquad_in_t *)call->in, (filter_biquad_params_t *)call->config,
        (filter_biquad_state_t *)call->state, call->info
    );
}

static void generation_dc_thunk(atom_call_t *call) {
    generation_dc_process(
        (generation_dc_out_t *)call->out, (generation_dc_in_t *)call->in, (generation_dc_params_t *)call->config,
        (generation_dc_state_t *)call->state, call->info
    );
}

static void generation_lfo_thunk(atom_call_t *call) {
    generation_lfo_process(
        (generation_lfo_out_t *)call->out, (generation_lfo_in_t *)call->in, (generation_lfo_params_t *)call->config,
        (generation_lfo_state_t *)call->state, call->info
    );
}

static void mix_wet_dry_thunk(atom_call_t *call) {
    mix_wet_dry_process(
        (mix_wet_dry_out_t *)call->out, (mix_wet_dry_in_t *)call->in, (mix_wet_dry_params_t *)call->config,
        (mix_wet_dry_state_t *)call->state, call->info
    );
}

static void modulation_amplitude_thunk(atom_call_t *call) {
    modulation_amplitude_process(
        (modulation_amplitude_out_t *)call->out, (modulation_amplitude_in_t *)call->in,
        (modulation_amplitude_params_t *)call->config, (modulation_amplitude_state_t *)call->state, call->info
    );
}

static void amplitude_smooth_thunk(atom_call_t *call) {
    amplitude_smooth_process(
        (amplitude_smooth_out_t *)call->out, (amplitude_smooth_in_t *)call->in,
        (amplitude_smooth_params_t *)call->config, (amplitude_smooth_state_t *)call->state, call->info
    );
}

static void detect_envelope_thunk(atom_call_t *call) {
    detect_envelope_process(
        (detect_envelope_out_t *)call->out, (detect_envelope_in_t *)call->in, (detect_envelope_params_t *)call->config,
        (detect_envelope_state_t *)call->state, call->info
    );
}

static void detect_peak_thunk(atom_call_t *call) {
    detect_peak_process(
        (detect_peak_out_t *)call->out, (detect_peak_in_t *)call->in, (detect_peak_params_t *)call->config,
        (detect_peak_state_t *)call->state, call->info
    );
}

static void detect_threshold_thunk(atom_call_t *call) {
    detect_threshold_process(
        (detect_threshold_out_t *)call->out, (detect_threshold_in_t *)call->in,
        (detect_threshold_params_t *)call->config, (detect_threshold_state_t *)call->state, call->info
    );
}

static void delay_unit_thunk(atom_call_t *call) {
    delay_unit_process(
        (delay_unit_out_t *)call->out, (delay_unit_in_t *)call->in, (delay_unit_params_t *)call->config,
        (delay_unit_state_t *)call->state, call->info
    );
}

static void delay_fractional_thunk(atom_call_t *call) {
    delay_fractional_process(
        (delay_fractional_out_t *)call->out, (delay_fractional_in_t *)call->in,
        (delay_fractional_params_t *)call->config, (delay_fractional_state_t *)call->state, call->info
    );
}

static void delay_tap_feedback_thunk(atom_call_t *call) {
    delay_tap_feedback_process(
        (delay_tap_feedback_out_t *)call->out, (delay_tap_feedback_in_t *)call->in,
        (delay_tap_feedback_params_t *)call->config, (delay_tap_feedback_state_t *)call->state, call->info
    );
}

static void delay_tap_feedforward_thunk(atom_call_t *call) {
    delay_tap_feedforward_process(
        (delay_tap_feedforward_out_t *)call->out, (delay_tap_feedforward_in_t *)call->in,
        (delay_tap_feedforward_params_t *)call->config, (delay_tap_feedforward_state_t *)call->state, call->info
    );
}

static void amplitude_add_thunk(atom_call_t *call) {
    amplitude_add_process(
        (amplitude_add_out_t *)call->out, (amplitude_add_in_t *)call->in, (amplitude_add_params_t *)call->config,
        (amplitude_add_state_t *)call->state, call->info
    );
}

static void amplitude_subtract_thunk(atom_call_t *call) {
    amplitude_subtract_process(
        (amplitude_subtract_out_t *)call->out, (amplitude_subtract_in_t *)call->in,
        (amplitude_subtract_params_t *)call->config, (amplitude_subtract_state_t *)call->state, call->info
    );
}

static void amplitude_divide_thunk(atom_call_t *call) {
    amplitude_divide_process(
        (amplitude_divide_out_t *)call->out, (amplitude_divide_in_t *)call->in,
        (amplitude_divide_params_t *)call->config, (amplitude_divide_state_t *)call->state, call->info
    );
}

static void mix_crossfade_thunk(atom_call_t *call) {
    mix_crossfade_process(
        (mix_crossfade_out_t *)call->out, (mix_crossfade_in_t *)call->in, (mix_crossfade_params_t *)call->config,
        (mix_crossfade_state_t *)call->state, call->info
    );
}

static void amplitude_clip_hard_thunk(atom_call_t *call) {
    amplitude_clip_hard_process(
        (amplitude_clip_hard_out_t *)call->out, (amplitude_clip_hard_in_t *)call->in,
        (amplitude_clip_hard_params_t *)call->config, (amplitude_clip_hard_state_t *)call->state, call->info
    );
}

static void amplitude_accumulate_thunk(atom_call_t *call) {
    amplitude_accumulate_process(
        (amplitude_accumulate_out_t *)call->out, (amplitude_accumulate_in_t *)call->in,
        (amplitude_accumulate_params_t *)call->config, (amplitude_accumulate_state_t *)call->state, call->info
    );
}

static void amplitude_latch_thunk(atom_call_t *call) {
    amplitude_latch_process(
        (amplitude_latch_out_t *)call->out, (amplitude_latch_in_t *)call->in, (amplitude_latch_params_t *)call->config,
        (amplitude_latch_state_t *)call->state, call->info
    );
}

static void amplitude_normalize_thunk(atom_call_t *call) {
    amplitude_normalize_process(
        (amplitude_normalize_out_t *)call->out, (amplitude_normalize_in_t *)call->in,
        (amplitude_normalize_params_t *)call->config, (amplitude_normalize_state_t *)call->state, call->info
    );
}

static void freq_window_thunk(atom_call_t *call) {
    freq_window_process(
        (freq_window_out_t *)call->out, (freq_window_in_t *)call->in, (freq_window_params_t *)call->config,
        (freq_window_state_t *)call->state, call->info
    );
}

static void freq_quantize_thunk(atom_call_t *call) {
    freq_quantize_process(
        (freq_quantize_out_t *)call->out, (freq_quantize_in_t *)call->in, (freq_quantize_params_t *)call->config,
        (freq_quantize_state_t *)call->state, call->info
    );
}

static void freq_shift_thunk(atom_call_t *call) {
    freq_shift_process(
        (freq_shift_out_t *)call->out, (freq_shift_in_t *)call->in, (freq_shift_params_t *)call->config,
        (freq_shift_state_t *)call->state, call->info
    );
}

static void freq_overlap_add_thunk(atom_call_t *call) {
    freq_overlap_add_process(
        (freq_overlap_add_out_t *)call->out, (freq_overlap_add_in_t *)call->in,
        (freq_overlap_add_params_t *)call->config, (freq_overlap_add_state_t *)call->state, call->info
    );
}

static void freq_overlap_save_thunk(atom_call_t *call) {
    freq_overlap_save_process(
        (freq_overlap_save_out_t *)call->out, (freq_overlap_save_in_t *)call->in,
        (freq_overlap_save_params_t *)call->config, (freq_overlap_save_state_t *)call->state, call->info
    );
}

static void interpolation_linear_thunk(atom_call_t *call) {
    interpolation_linear_process(
        (interpolation_linear_out_t *)call->out, (interpolation_linear_in_t *)call->in,
        (interpolation_linear_params_t *)call->config, (interpolation_linear_state_t *)call->state, call->info
    );
}

static void interpolation_cubic_thunk(atom_call_t *call) {
    interpolation_cubic_process(
        (interpolation_cubic_out_t *)call->out, (interpolation_cubic_in_t *)call->in,
        (interpolation_cubic_params_t *)call->config, (interpolation_cubic_state_t *)call->state, call->info
    );
}

static void interpolation_lagrange_thunk(atom_call_t *call) {
    interpolation_lagrange_process(
        (interpolation_lagrange_out_t *)call->out, (interpolation_lagrange_in_t *)call->in,
        (interpolation_lagrange_params_t *)call->config, (interpolation_lagrange_state_t *)call->state, call->info
    );
}

static void interpolation_sinc_thunk(atom_call_t *call) {
    interpolation_sinc_process(
        (interpolation_sinc_out_t *)call->out, (interpolation_sinc_in_t *)call->in,
        (interpolation_sinc_params_t *)call->config, (interpolation_sinc_state_t *)call->state, call->info
    );
}

static void src_convert_format_thunk(atom_call_t *call) {
    src_convert_format_process(
        (src_convert_format_out_t *)call->out, (src_convert_format_in_t *)call->in,
        (src_convert_format_params_t *)call->config, (src_convert_format_state_t *)call->state, call->info
    );
}

static void src_downsample_thunk(atom_call_t *call) {
    src_downsample_process(
        (src_downsample_out_t *)call->out, (src_downsample_in_t *)call->in, (src_downsample_params_t *)call->config,
        (src_downsample_state_t *)call->state, call->info
    );
}

static void src_upsample_thunk(atom_call_t *call) {
    src_upsample_process(
        (src_upsample_out_t *)call->out, (src_upsample_in_t *)call->in, (src_upsample_params_t *)call->config,
        (src_upsample_state_t *)call->state, call->info
    );
}

static void src_antialias_thunk(atom_call_t *call) {
    src_antialias_process(
        (src_antialias_out_t *)call->out, (src_antialias_in_t *)call->in, (src_antialias_params_t *)call->config,
        (src_antialias_state_t *)call->state, call->info
    );
}

static void src_antiimage_thunk(atom_call_t *call) {
    src_antiimage_process(
        (src_antiimage_out_t *)call->out, (src_antiimage_in_t *)call->in, (src_antiimage_params_t *)call->config,
        (src_antiimage_state_t *)call->state, call->info
    );
}

static void generation_impulse_thunk(atom_call_t *call) {
    generation_impulse_process(
        (generation_impulse_out_t *)call->out, (generation_impulse_in_t *)call->in,
        (generation_impulse_params_t *)call->config, (generation_impulse_state_t *)call->state, call->info
    );
}

static void generation_noise_thunk(atom_call_t *call) {
    generation_noise_process(
        (generation_noise_out_t *)call->out, (generation_noise_in_t *)call->in,
        (generation_noise_params_t *)call->config, (generation_noise_state_t *)call->state, call->info
    );
}

static void generation_envelope_thunk(atom_call_t *call) {
    generation_envelope_process(
        (generation_envelope_out_t *)call->out, (generation_envelope_in_t *)call->in,
        (generation_envelope_params_t *)call->config, (generation_envelope_state_t *)call->state, call->info
    );
}

static void generation_oscillator_thunk(atom_call_t *call) {
    generation_oscillator_process(
        (generation_oscillator_out_t *)call->out, (generation_oscillator_in_t *)call->in,
        (generation_oscillator_params_t *)call->config, (generation_oscillator_state_t *)call->state, call->info
    );
}

static void modulation_ring_thunk(atom_call_t *call) {
    modulation_ring_process(
        (modulation_ring_out_t *)call->out, (modulation_ring_in_t *)call->in, (modulation_ring_params_t *)call->config,
        (modulation_ring_state_t *)call->state, call->info
    );
}

static void modulation_frequency_thunk(atom_call_t *call) {
    modulation_frequency_process(
        (modulation_frequency_out_t *)call->out, (modulation_frequency_in_t *)call->in,
        (modulation_frequency_params_t *)call->config, (modulation_frequency_state_t *)call->state, call->info
    );
}

static void modulation_phase_thunk(atom_call_t *call) {
    modulation_phase_process(
        (modulation_phase_out_t *)call->out, (modulation_phase_in_t *)call->in,
        (modulation_phase_params_t *)call->config, (modulation_phase_state_t *)call->state, call->info
    );
}

static void modulation_scrub_thunk(atom_call_t *call) {
    modulation_scrub_process(
        (modulation_scrub_out_t *)call->out, (modulation_scrub_in_t *)call->in,
        (modulation_scrub_params_t *)call->config, (modulation_scrub_state_t *)call->state, call->info
    );
}

static void detect_slope_thunk(atom_call_t *call) {
    detect_slope_process(
        (detect_slope_out_t *)call->out, (detect_slope_in_t *)call->in, (detect_slope_params_t *)call->config,
        (detect_slope_state_t *)call->state, call->info
    );
}

static void detect_rms_thunk(atom_call_t *call) {
    detect_rms_process(
        (detect_rms_out_t *)call->out, (detect_rms_in_t *)call->in, (detect_rms_params_t *)call->config,
        (detect_rms_state_t *)call->state, call->info
    );
}

static void detect_zero_crossing_thunk(atom_call_t *call) {
    detect_zero_crossing_process(
        (detect_zero_crossing_out_t *)call->out, (detect_zero_crossing_in_t *)call->in,
        (detect_zero_crossing_params_t *)call->config, (detect_zero_crossing_state_t *)call->state, call->info
    );
}

static void detect_autocorrelate_thunk(atom_call_t *call) {
    detect_autocorrelate_process(
        (detect_autocorrelate_out_t *)call->out, (detect_autocorrelate_in_t *)call->in,
        (detect_autocorrelate_params_t *)call->config, (detect_autocorrelate_state_t *)call->state, call->info
    );
}

static void detect_pitch_thunk(atom_call_t *call) {
    detect_pitch_process(
        (detect_pitch_out_t *)call->out, (detect_pitch_in_t *)call->in, (detect_pitch_params_t *)call->config,
        (detect_pitch_state_t *)call->state, call->info
    );
}

static void filter_allpass_thunk(atom_call_t *call) {
    filter_allpass_process(
        (filter_allpass_out_t *)call->out, (filter_allpass_in_t *)call->in, (filter_allpass_params_t *)call->config,
        (filter_allpass_state_t *)call->state, call->info
    );
}

static void filter_comb_ff_thunk(atom_call_t *call) {
    filter_comb_ff_process(
        (filter_comb_ff_out_t *)call->out, (filter_comb_ff_in_t *)call->in, (filter_comb_ff_params_t *)call->config,
        (filter_comb_ff_state_t *)call->state, call->info
    );
}

static void filter_comb_fb_thunk(atom_call_t *call) {
    filter_comb_fb_process(
        (filter_comb_fb_out_t *)call->out, (filter_comb_fb_in_t *)call->in, (filter_comb_fb_params_t *)call->config,
        (filter_comb_fb_state_t *)call->state, call->info
    );
}

static void filter_dc_block_thunk(atom_call_t *call) {
    filter_dc_block_process(
        (filter_dc_block_out_t *)call->out, (filter_dc_block_in_t *)call->in, (filter_dc_block_params_t *)call->config,
        (filter_dc_block_state_t *)call->state, call->info
    );
}

static void filter_differentiate_thunk(atom_call_t *call) {
    filter_differentiate_process(
        (filter_differentiate_out_t *)call->out, (filter_differentiate_in_t *)call->in,
        (filter_differentiate_params_t *)call->config, (filter_differentiate_state_t *)call->state, call->info
    );
}

static void filter_integrate_thunk(atom_call_t *call) {
    filter_integrate_process(
        (filter_integrate_out_t *)call->out, (filter_integrate_in_t *)call->in,
        (filter_integrate_params_t *)call->config, (filter_integrate_state_t *)call->state, call->info
    );
}

static void filter_fir_thunk(atom_call_t *call) {
    filter_fir_process(
        (filter_fir_out_t *)call->out, (filter_fir_in_t *)call->in, (filter_fir_params_t *)call->config,
        (filter_fir_state_t *)call->state, call->info
    );
}

static void mix_matrix_thunk(atom_call_t *call) {
    mix_matrix_process(
        (mix_matrix_out_t *)call->out, (mix_matrix_in_t *)call->in, (mix_matrix_params_t *)call->config,
        (mix_matrix_state_t *)call->state, call->info
    );
}

static void mix_pan_stereo_thunk(atom_call_t *call) {
    mix_pan_stereo_process(
        (mix_pan_stereo_out_t *)call->out, (mix_pan_stereo_in_t *)call->in, (mix_pan_stereo_params_t *)call->config,
        (mix_pan_stereo_state_t *)call->state, call->info
    );
}

static void mix_encode_ms_thunk(atom_call_t *call) {
    mix_encode_ms_process(
        (mix_encode_ms_out_t *)call->out, (mix_encode_ms_in_t *)call->in, (mix_encode_ms_params_t *)call->config,
        (mix_encode_ms_state_t *)call->state, call->info
    );
}

static void mix_decode_ms_thunk(atom_call_t *call) {
    mix_decode_ms_process(
        (mix_decode_ms_out_t *)call->out, (mix_decode_ms_in_t *)call->in, (mix_decode_ms_params_t *)call->config,
        (mix_decode_ms_state_t *)call->state, call->info
    );
}

static void nonlinear_bitcrush_thunk(atom_call_t *call) {
    nonlinear_bitcrush_process(
        (nonlinear_bitcrush_out_t *)call->out, (nonlinear_bitcrush_in_t *)call->in,
        (nonlinear_bitcrush_params_t *)call->config, (nonlinear_bitcrush_state_t *)call->state, call->info
    );
}

static void nonlinear_waveshape_thunk(atom_call_t *call) {
    nonlinear_waveshape_process(
        (nonlinear_waveshape_out_t *)call->out, (nonlinear_waveshape_in_t *)call->in,
        (nonlinear_waveshape_params_t *)call->config, (nonlinear_waveshape_state_t *)call->state, call->info
    );
}

static void nonlinear_samplerate_reduce_thunk(atom_call_t *call) {
    nonlinear_samplerate_reduce_process(
        (nonlinear_samplerate_reduce_out_t *)call->out, (nonlinear_samplerate_reduce_in_t *)call->in,
        (nonlinear_samplerate_reduce_params_t *)call->config, (nonlinear_samplerate_reduce_state_t *)call->state,
        call->info
    );
}

REGISTER_ALL(THUNK)

// ─────────────────────────────────────────────
// Field Descriptors
// ─────────────────────────────────────────────

static const atom_field_desc_t amplitude_accumulate_state_fields[] = {
    {"accumulator", FIELD_FLOAT, offsetof(amplitude_accumulate_state_t, accumulator)},
};

static const atom_field_desc_t amplitude_clip_hard_config_fields[] = {
    {"threshold", FIELD_FLOAT, offsetof(amplitude_clip_hard_params_t, threshold)},
};

static const atom_field_desc_t amplitude_clip_soft_config_fields[] = {
    {"threshold", FIELD_FLOAT, offsetof(amplitude_clip_soft_params_t, threshold)},
    {    "curve",   FIELD_INT, offsetof(amplitude_clip_soft_params_t,     curve)},
};

static const atom_field_desc_t amplitude_divide_config_fields[] = {
    {"epsilon", FIELD_FLOAT, offsetof(amplitude_divide_params_t, epsilon)},
};

static const atom_field_desc_t amplitude_latch_config_fields[] = {
    {"threshold", FIELD_FLOAT, offsetof(amplitude_latch_params_t, threshold)},
};

static const atom_field_desc_t amplitude_latch_state_fields[] = {
    {"latched_value", FIELD_FLOAT, offsetof(amplitude_latch_state_t, latched_value)},
    {    "prev_gate",   FIELD_INT, offsetof(amplitude_latch_state_t,     prev_gate)},
};

static const atom_field_desc_t amplitude_normalize_config_fields[] = {
    {"target_level", FIELD_FLOAT, offsetof(amplitude_normalize_params_t, target_level)},
    {        "mode",   FIELD_INT, offsetof(amplitude_normalize_params_t,         mode)},
};

static const atom_field_desc_t amplitude_normalize_state_fields[] = {
    {"running_peak", FIELD_FLOAT, offsetof(amplitude_normalize_state_t, running_peak)},
};

static const atom_field_desc_t amplitude_smooth_config_fields[] = {
    {     "attack", FIELD_FLOAT, offsetof(amplitude_smooth_params_t,      attack)},
    {    "release", FIELD_FLOAT, offsetof(amplitude_smooth_params_t,     release)},
    {"sample_rate", FIELD_FLOAT, offsetof(amplitude_smooth_params_t, sample_rate)},
};

static const atom_field_desc_t amplitude_smooth_state_fields[] = {
    {"prev_value", FIELD_FLOAT, offsetof(amplitude_smooth_state_t, prev_value)},
};

static const atom_field_desc_t delay_fractional_config_fields[] = {
    {"delay_samples", FIELD_FLOAT, offsetof(delay_fractional_params_t, delay_samples)},
    {"interpolation",   FIELD_INT, offsetof(delay_fractional_params_t, interpolation)},
};

static const atom_field_desc_t delay_fractional_state_fields[] = {
    {   "buffer", FIELD_BUFFER, offsetof(delay_fractional_state_t,    buffer)},
    {"write_pos",    FIELD_INT, offsetof(delay_fractional_state_t, write_pos)},
};

static const atom_field_desc_t delay_line_config_fields[] = {
    {"length", FIELD_INT, offsetof(delay_line_params_t, length)},
};

static const atom_field_desc_t delay_line_state_fields[] = {
    {   "buffer", FIELD_BUFFER, offsetof(delay_line_state_t,    buffer)},
    {"write_pos",    FIELD_INT, offsetof(delay_line_state_t, write_pos)},
};

static const atom_field_desc_t delay_tap_feedback_config_fields[] = {
    {"coefficient", FIELD_FLOAT, offsetof(delay_tap_feedback_params_t, coefficient)},
};

static const atom_field_desc_t delay_tap_feedforward_config_fields[] = {
    {"coefficient", FIELD_FLOAT, offsetof(delay_tap_feedforward_params_t, coefficient)},
};

static const atom_field_desc_t delay_unit_state_fields[] = {
    {"prev_sample", FIELD_FLOAT, offsetof(delay_unit_state_t, prev_sample)},
};

static const atom_field_desc_t detect_autocorrelate_config_fields[] = {
    {"max_lag", FIELD_INT, offsetof(detect_autocorrelate_params_t, max_lag)},
};

static const atom_field_desc_t detect_autocorrelate_state_fields[] = {
    {   "buffer", FIELD_BUFFER, offsetof(detect_autocorrelate_state_t,    buffer)},
    {"write_pos",    FIELD_INT, offsetof(detect_autocorrelate_state_t, write_pos)},
};

static const atom_field_desc_t detect_pitch_config_fields[] = {
    {    "max_lag",   FIELD_INT, offsetof(detect_pitch_params_t,     max_lag)},
    {"sample_rate", FIELD_FLOAT, offsetof(detect_pitch_params_t, sample_rate)},
};
static const atom_field_desc_t detect_pitch_state_fields[] = {
    {   "buffer", FIELD_BUFFER, offsetof(detect_pitch_state_t,    buffer)},
    {"write_pos",    FIELD_INT, offsetof(detect_pitch_state_t, write_pos)},
};

static const atom_field_desc_t freq_shift_config_fields[] = {
    {"block_size", FIELD_INT, offsetof(freq_shift_params_t, block_size)},
};
static const atom_field_desc_t freq_shift_state_fields[] = {
    {   "window", FIELD_BUFFER, offsetof(freq_shift_state_t,    window)},
    {     "real", FIELD_BUFFER, offsetof(freq_shift_state_t,      real)},
    {     "imag", FIELD_BUFFER, offsetof(freq_shift_state_t,      imag)},
    {"write_pos",    FIELD_INT, offsetof(freq_shift_state_t, write_pos)},
    { "read_ptr",  FIELD_FLOAT, offsetof(freq_shift_state_t,  read_ptr)},
};

static const atom_field_desc_t detect_envelope_config_fields[] = {
    {     "attack", FIELD_FLOAT, offsetof(detect_envelope_params_t,      attack)},
    {    "release", FIELD_FLOAT, offsetof(detect_envelope_params_t,     release)},
    {"sample_rate", FIELD_FLOAT, offsetof(detect_envelope_params_t, sample_rate)},
};

static const atom_field_desc_t detect_envelope_state_fields[] = {
    {"prev_envelope", FIELD_FLOAT, offsetof(detect_envelope_state_t, prev_envelope)},
};

static const atom_field_desc_t detect_peak_config_fields[] = {
    {     "attack", FIELD_FLOAT, offsetof(detect_peak_params_t,      attack)},
    {    "release", FIELD_FLOAT, offsetof(detect_peak_params_t,     release)},
    {"sample_rate", FIELD_FLOAT, offsetof(detect_peak_params_t, sample_rate)},
};

static const atom_field_desc_t detect_peak_state_fields[] = {
    {"prev_peak", FIELD_FLOAT, offsetof(detect_peak_state_t, prev_peak)},
};

static const atom_field_desc_t detect_rms_config_fields[] = {
    {"window_size", FIELD_INT, offsetof(detect_rms_params_t, window_size)},
};

static const atom_field_desc_t detect_rms_state_fields[] = {
    {   "buffer", FIELD_BUFFER, offsetof(detect_rms_state_t,    buffer)},
    {"write_pos",    FIELD_INT, offsetof(detect_rms_state_t, write_pos)},
    {      "sum",  FIELD_FLOAT, offsetof(detect_rms_state_t,       sum)},
};

static const atom_field_desc_t detect_slope_state_fields[] = {
    {"prev_sample", FIELD_FLOAT, offsetof(detect_slope_state_t, prev_sample)},
};

static const atom_field_desc_t detect_threshold_config_fields[] = {
    {"threshold", FIELD_FLOAT, offsetof(detect_threshold_params_t, threshold)},
};

static const atom_field_desc_t detect_zero_crossing_state_fields[] = {
    {"prev_sample", FIELD_FLOAT, offsetof(detect_zero_crossing_state_t, prev_sample)},
};

static const atom_field_desc_t filter_allpass_config_fields[] = {
    {"delay_samples",   FIELD_INT, offsetof(filter_allpass_params_t, delay_samples)},
    {  "coefficient", FIELD_FLOAT, offsetof(filter_allpass_params_t,   coefficient)},
};

static const atom_field_desc_t filter_allpass_state_fields[] = {
    {   "buffer", FIELD_BUFFER, offsetof(filter_allpass_state_t,    buffer)},
    {"write_pos",    FIELD_INT, offsetof(filter_allpass_state_t, write_pos)},
};

static const atom_field_desc_t filter_biquad_config_fields[] = {
    {"b0", FIELD_FLOAT, offsetof(filter_biquad_params_t, b0)},
    {"b1", FIELD_FLOAT, offsetof(filter_biquad_params_t, b1)},
    {"b2", FIELD_FLOAT, offsetof(filter_biquad_params_t, b2)},
    {"a1", FIELD_FLOAT, offsetof(filter_biquad_params_t, a1)},
    {"a2", FIELD_FLOAT, offsetof(filter_biquad_params_t, a2)},
};

static const atom_field_desc_t filter_biquad_state_fields[] = {
    {"z1", FIELD_FLOAT, offsetof(filter_biquad_state_t, z1)},
    {"z2", FIELD_FLOAT, offsetof(filter_biquad_state_t, z2)},
};

static const atom_field_desc_t filter_comb_fb_config_fields[] = {
    {"delay_samples",   FIELD_INT, offsetof(filter_comb_fb_params_t, delay_samples)},
    {  "coefficient", FIELD_FLOAT, offsetof(filter_comb_fb_params_t,   coefficient)},
};

static const atom_field_desc_t filter_comb_fb_state_fields[] = {
    {   "buffer", FIELD_BUFFER, offsetof(filter_comb_fb_state_t,    buffer)},
    {"write_pos",    FIELD_INT, offsetof(filter_comb_fb_state_t, write_pos)},
};

static const atom_field_desc_t filter_comb_ff_config_fields[] = {
    {"delay_samples",   FIELD_INT, offsetof(filter_comb_ff_params_t, delay_samples)},
    {  "coefficient", FIELD_FLOAT, offsetof(filter_comb_ff_params_t,   coefficient)},
};

static const atom_field_desc_t filter_comb_ff_state_fields[] = {
    {   "buffer", FIELD_BUFFER, offsetof(filter_comb_ff_state_t,    buffer)},
    {"write_pos",    FIELD_INT, offsetof(filter_comb_ff_state_t, write_pos)},
};

static const atom_field_desc_t filter_dc_block_config_fields[] = {
    {"coefficient", FIELD_FLOAT, offsetof(filter_dc_block_params_t, coefficient)},
};

static const atom_field_desc_t filter_dc_block_state_fields[] = {
    { "prev_input", FIELD_FLOAT, offsetof(filter_dc_block_state_t,  prev_input)},
    {"prev_output", FIELD_FLOAT, offsetof(filter_dc_block_state_t, prev_output)},
};

static const atom_field_desc_t filter_differentiate_state_fields[] = {
    {"prev_sample", FIELD_FLOAT, offsetof(filter_differentiate_state_t, prev_sample)},
};

static const atom_field_desc_t filter_fir_config_fields[] = {
    {     "kernel", FIELD_BUFFER, offsetof(filter_fir_params_t,      kernel)},
    {"kernel_size",    FIELD_INT, offsetof(filter_fir_params_t, kernel_size)},
};

static const atom_field_desc_t filter_fir_state_fields[] = {
    {   "buffer", FIELD_BUFFER, offsetof(filter_fir_state_t,    buffer)},
    {"write_pos",    FIELD_INT, offsetof(filter_fir_state_t, write_pos)},
};

static const atom_field_desc_t filter_integrate_state_fields[] = {
    {"accumulator", FIELD_FLOAT, offsetof(filter_integrate_state_t, accumulator)},
};

static const atom_field_desc_t freq_fft_config_fields[] = {
    {"block_size", FIELD_INT, offsetof(freq_fft_params_t, block_size)},
};

static const atom_field_desc_t freq_ifft_config_fields[] = {
    {"block_size", FIELD_INT, offsetof(freq_ifft_params_t, block_size)},
};

static const atom_field_desc_t freq_multiply_config_fields[] = {
    {"block_size", FIELD_INT, offsetof(freq_multiply_params_t, block_size)},
};

static const atom_field_desc_t freq_overlap_add_config_fields[] = {
    {"block_size", FIELD_INT, offsetof(freq_overlap_add_params_t, block_size)},
    {  "hop_size", FIELD_INT, offsetof(freq_overlap_add_params_t,   hop_size)},
};

static const atom_field_desc_t freq_overlap_add_state_fields[] = {
    {"buffer", FIELD_BUFFER, offsetof(freq_overlap_add_state_t, buffer)},
};

static const atom_field_desc_t freq_overlap_save_config_fields[] = {
    {"block_size", FIELD_INT, offsetof(freq_overlap_save_params_t, block_size)},
    {  "hop_size", FIELD_INT, offsetof(freq_overlap_save_params_t,   hop_size)},
};

static const atom_field_desc_t freq_overlap_save_state_fields[] = {
    {   "buffer", FIELD_BUFFER, offsetof(freq_overlap_save_state_t,    buffer)},
    {"write_pos",    FIELD_INT, offsetof(freq_overlap_save_state_t, write_pos)},
};

static const atom_field_desc_t freq_window_config_fields[] = {
    {"window_type", FIELD_INT, offsetof(freq_window_params_t, window_type)},
    { "block_size", FIELD_INT, offsetof(freq_window_params_t,  block_size)},
};

static const atom_field_desc_t generation_dc_config_fields[] = {
    {"value", FIELD_FLOAT, offsetof(generation_dc_params_t, value)},
};

static const atom_field_desc_t generation_envelope_config_fields[] = {
    {     "attack", FIELD_FLOAT, offsetof(generation_envelope_params_t,      attack)},
    {      "decay", FIELD_FLOAT, offsetof(generation_envelope_params_t,       decay)},
    {    "sustain", FIELD_FLOAT, offsetof(generation_envelope_params_t,     sustain)},
    {    "release", FIELD_FLOAT, offsetof(generation_envelope_params_t,     release)},
    {"sample_rate", FIELD_FLOAT, offsetof(generation_envelope_params_t, sample_rate)},
};

static const atom_field_desc_t generation_envelope_state_fields[] = {
    {"current_level", FIELD_FLOAT, offsetof(generation_envelope_state_t, current_level)},
    {        "stage",   FIELD_INT, offsetof(generation_envelope_state_t,         stage)},
};

static const atom_field_desc_t generation_impulse_config_fields[] = {
    {   "interval", FIELD_FLOAT, offsetof(generation_impulse_params_t,    interval)},
    {"sample_rate", FIELD_FLOAT, offsetof(generation_impulse_params_t, sample_rate)},
};

static const atom_field_desc_t generation_impulse_state_fields[] = {
    {"counter", FIELD_INT, offsetof(generation_impulse_state_t, counter)},
};

static const atom_field_desc_t generation_lfo_config_fields[] = {
    {   "frequency", FIELD_FLOAT, offsetof(generation_lfo_params_t,    frequency)},
    {    "waveform",   FIELD_INT, offsetof(generation_lfo_params_t,     waveform)},
    {"phase_offset", FIELD_FLOAT, offsetof(generation_lfo_params_t, phase_offset)},
    { "sample_rate", FIELD_FLOAT, offsetof(generation_lfo_params_t,  sample_rate)},
};

static const atom_field_desc_t generation_lfo_state_fields[] = {
    {"phase", FIELD_FLOAT, offsetof(generation_lfo_state_t, phase)},
};

static const atom_field_desc_t generation_noise_config_fields[] = {
    {"amplitude", FIELD_FLOAT, offsetof(generation_noise_params_t, amplitude)},
    {    "color",   FIELD_INT, offsetof(generation_noise_params_t,     color)},
};

static const atom_field_desc_t generation_noise_state_fields[] = {
    {      "seed",   FIELD_INT, offsetof(generation_noise_state_t,       seed)},
    {"prev_value", FIELD_FLOAT, offsetof(generation_noise_state_t, prev_value)},
};

static const atom_field_desc_t generation_oscillator_config_fields[] = {
    {   "frequency", FIELD_FLOAT, offsetof(generation_oscillator_params_t,    frequency)},
    {    "waveform",   FIELD_INT, offsetof(generation_oscillator_params_t,     waveform)},
    {"phase_offset", FIELD_FLOAT, offsetof(generation_oscillator_params_t, phase_offset)},
    { "sample_rate", FIELD_FLOAT, offsetof(generation_oscillator_params_t,  sample_rate)},
};

static const atom_field_desc_t generation_oscillator_state_fields[] = {
    {"phase", FIELD_FLOAT, offsetof(generation_oscillator_state_t, phase)},
};

static const atom_field_desc_t interpolation_lagrange_config_fields[] = {
    {"order", FIELD_INT, offsetof(interpolation_lagrange_params_t, order)},
};

static const atom_field_desc_t interpolation_lagrange_state_fields[] = {
    {   "buffer", FIELD_BUFFER, offsetof(interpolation_lagrange_state_t,    buffer)},
    {"write_pos",    FIELD_INT, offsetof(interpolation_lagrange_state_t, write_pos)},
};

static const atom_field_desc_t interpolation_sinc_config_fields[] = {
    {"num_taps", FIELD_INT, offsetof(interpolation_sinc_params_t, num_taps)},
};

static const atom_field_desc_t interpolation_sinc_state_fields[] = {
    {"taps", FIELD_BUFFER, offsetof(interpolation_sinc_state_t, taps)},
};

static const atom_field_desc_t mix_crossfade_config_fields[] = {
    {"t", FIELD_FLOAT, offsetof(mix_crossfade_params_t, t)},
};

static const atom_field_desc_t mix_matrix_config_fields[] = {
    { "num_in", FIELD_INT, offsetof(mix_matrix_params_t,  num_in)},
    {"num_out", FIELD_INT, offsetof(mix_matrix_params_t, num_out)},
};

static const atom_field_desc_t mix_pan_stereo_config_fields[] = {
    {"position", FIELD_FLOAT, offsetof(mix_pan_stereo_params_t, position)},
};

static const atom_field_desc_t mix_wet_dry_config_fields[] = {
    {"mix", FIELD_FLOAT, offsetof(mix_wet_dry_params_t, mix)},
};

static const atom_field_desc_t modulation_amplitude_config_fields[] = {
    {"depth", FIELD_FLOAT, offsetof(modulation_amplitude_params_t, depth)},
};

static const atom_field_desc_t modulation_frequency_config_fields[] = {
    {"depth", FIELD_FLOAT, offsetof(modulation_frequency_params_t, depth)},
};

static const atom_field_desc_t modulation_frequency_state_fields[] = {
    {       "buffer", FIELD_BUFFER, offsetof(modulation_frequency_state_t,        buffer)},
    {    "write_pos",    FIELD_INT, offsetof(modulation_frequency_state_t,     write_pos)},
    {"current_delay",  FIELD_FLOAT, offsetof(modulation_frequency_state_t, current_delay)},
};

static const atom_field_desc_t modulation_phase_config_fields[] = {
    {"depth", FIELD_FLOAT, offsetof(modulation_phase_params_t, depth)},
};

static const atom_field_desc_t modulation_phase_state_fields[] = {
    {   "buffer", FIELD_BUFFER, offsetof(modulation_phase_state_t,    buffer)},
    {"write_pos",    FIELD_INT, offsetof(modulation_phase_state_t, write_pos)},
};

static const atom_field_desc_t modulation_scrub_config_fields[] = {
    {"buffer_size", FIELD_INT, offsetof(modulation_scrub_params_t, buffer_size)},
};

static const atom_field_desc_t nonlinear_bitcrush_config_fields[] = {
    {"bit_depth", FIELD_FLOAT, offsetof(nonlinear_bitcrush_params_t, bit_depth)},
};

static const atom_field_desc_t nonlinear_samplerate_reduce_config_fields[] = {
    {"factor", FIELD_FLOAT, offsetof(nonlinear_samplerate_reduce_params_t, factor)},
};

static const atom_field_desc_t nonlinear_samplerate_reduce_state_fields[] = {
    {"last_val", FIELD_FLOAT, offsetof(nonlinear_samplerate_reduce_state_t, last_val)},
    { "counter", FIELD_FLOAT, offsetof(nonlinear_samplerate_reduce_state_t,  counter)},
};

static const atom_field_desc_t nonlinear_waveshape_config_fields[] = {
    {"transfer_table", FIELD_BUFFER, offsetof(nonlinear_waveshape_params_t, transfer_table)},
    {    "table_size",    FIELD_INT, offsetof(nonlinear_waveshape_params_t,     table_size)},
};

static const atom_field_desc_t src_antialias_config_fields[] = {
    {     "cutoff", FIELD_FLOAT, offsetof(src_antialias_params_t,      cutoff)},
    {"sample_rate", FIELD_FLOAT, offsetof(src_antialias_params_t, sample_rate)},
};

static const atom_field_desc_t src_antialias_state_fields[] = {
    {"z1", FIELD_FLOAT, offsetof(src_antialias_state_t, z1)},
    {"z2", FIELD_FLOAT, offsetof(src_antialias_state_t, z2)},
};

static const atom_field_desc_t src_antiimage_config_fields[] = {
    {     "cutoff", FIELD_FLOAT, offsetof(src_antiimage_params_t,      cutoff)},
    {"sample_rate", FIELD_FLOAT, offsetof(src_antiimage_params_t, sample_rate)},
};

static const atom_field_desc_t src_antiimage_state_fields[] = {
    {"z1", FIELD_FLOAT, offsetof(src_antiimage_state_t, z1)},
    {"z2", FIELD_FLOAT, offsetof(src_antiimage_state_t, z2)},
};

static const atom_field_desc_t src_convert_format_config_fields[] = {
    {"from_format", FIELD_INT, offsetof(src_convert_format_params_t, from_format)},
    {  "to_format", FIELD_INT, offsetof(src_convert_format_params_t,   to_format)},
};

static const atom_field_desc_t src_downsample_config_fields[] = {
    {"factor", FIELD_INT, offsetof(src_downsample_params_t, factor)},
};

static const atom_field_desc_t src_upsample_config_fields[] = {
    {"factor", FIELD_INT, offsetof(src_upsample_params_t, factor)},
};

// ─────────────────────────────────────────────
// Registry table
// ─────────────────────────────────────────────

#define ENTRY_WITH_FIELDS(atom_name, c_fields, n_cfg, s_fields, n_st) \
    {                                                                 \
        .name            = #atom_name,                                \
        .thunk           = atom_name##_thunk,                         \
        .out_size        = sizeof(atom_name##_out_t),                 \
        .in_size         = sizeof(atom_name##_in_t),                  \
        .config_size     = sizeof(atom_name##_params_t),              \
        .state_size      = sizeof(atom_name##_state_t),               \
        .config_fields   = c_fields,                                  \
        .n_config_fields = n_cfg,                                     \
        .state_fields    = s_fields,                                  \
        .n_state_fields  = n_st,                                      \
    },

static atom_registry_entry_t g_registry[] = {
    ENTRY_WITH_FIELDS(amplitude_accumulate, NULL, 0, amplitude_accumulate_state_fields, sizeof(amplitude_accumulate_state_fields) / sizeof(amplitude_accumulate_state_fields[0])) ENTRY_WITH_FIELDS(
        amplitude_latch,
        amplitude_latch_config_fields,
        sizeof(amplitude_latch_config_fields) / sizeof(amplitude_latch_config_fields[0]),
        amplitude_latch_state_fields,
        sizeof(amplitude_latch_state_fields) / sizeof(amplitude_latch_state_fields[0])
    ) ENTRY_WITH_FIELDS(amplitude_add, NULL, 0, NULL, 0)
        ENTRY_WITH_FIELDS(amplitude_clip_hard, amplitude_clip_hard_config_fields, sizeof(amplitude_clip_hard_config_fields) / sizeof(amplitude_clip_hard_config_fields[0]), NULL, 0) ENTRY_WITH_FIELDS(
            amplitude_clip_soft,
            amplitude_clip_soft_config_fields,
            sizeof(amplitude_clip_soft_config_fields) / sizeof(amplitude_clip_soft_config_fields[0]),
            NULL,
            0
        ) ENTRY_WITH_FIELDS(amplitude_divide, amplitude_divide_config_fields, sizeof(amplitude_divide_config_fields) / sizeof(amplitude_divide_config_fields[0]), NULL, 0)
            ENTRY_WITH_FIELDS(amplitude_multiply, NULL, 0, NULL, 0) ENTRY_WITH_FIELDS(
                amplitude_normalize,
                amplitude_normalize_config_fields,
                sizeof(amplitude_normalize_config_fields) / sizeof(amplitude_normalize_config_fields[0]),
                amplitude_normalize_state_fields,
                sizeof(amplitude_normalize_state_fields) / sizeof(amplitude_normalize_state_fields[0])
            )
                ENTRY_WITH_FIELDS(
                    amplitude_smooth,
                    amplitude_smooth_config_fields,
                    sizeof(amplitude_smooth_config_fields) / sizeof(amplitude_smooth_config_fields[0]),
                    amplitude_smooth_state_fields,
                    sizeof(amplitude_smooth_state_fields) /
                        sizeof(amplitude_smooth_state_fields[0])
                ) ENTRY_WITH_FIELDS(amplitude_subtract, NULL, 0, NULL, 0)
                    ENTRY_WITH_FIELDS(
                        delay_fractional,
                        delay_fractional_config_fields,
                        sizeof(delay_fractional_config_fields) / sizeof(delay_fractional_config_fields[0]),
                        delay_fractional_state_fields,
                        sizeof(delay_fractional_state_fields) /
                            sizeof(delay_fractional_state_fields[0])
                    )
                        ENTRY_WITH_FIELDS(
                            delay_line,
                            delay_line_config_fields,
                            sizeof(delay_line_config_fields) / sizeof(delay_line_config_fields[0]),
                            delay_line_state_fields,
                            sizeof(delay_line_state_fields) /
                                sizeof(delay_line_state_fields[0])
                        ) ENTRY_WITH_FIELDS(delay_tap_feedback, delay_tap_feedback_config_fields, sizeof(delay_tap_feedback_config_fields) / sizeof(delay_tap_feedback_config_fields[0]), NULL, 0) ENTRY_WITH_FIELDS(delay_tap_feedforward, delay_tap_feedforward_config_fields, sizeof(delay_tap_feedforward_config_fields) / sizeof(delay_tap_feedforward_config_fields[0]), NULL, 0) ENTRY_WITH_FIELDS(delay_unit, NULL, 0, delay_unit_state_fields, sizeof(delay_unit_state_fields) / sizeof(delay_unit_state_fields[0])) ENTRY_WITH_FIELDS(detect_autocorrelate, detect_autocorrelate_config_fields, sizeof(detect_autocorrelate_config_fields) / sizeof(detect_autocorrelate_config_fields[0]), detect_autocorrelate_state_fields, sizeof(detect_autocorrelate_state_fields) / sizeof(detect_autocorrelate_state_fields[0])) ENTRY_WITH_FIELDS(detect_pitch, detect_pitch_config_fields, sizeof(detect_pitch_config_fields) / sizeof(detect_pitch_config_fields[0]), detect_pitch_state_fields, sizeof(detect_pitch_state_fields) / sizeof(detect_pitch_state_fields[0]))

                            ENTRY_WITH_FIELDS(
                                detect_envelope,
                                detect_envelope_config_fields,
                                sizeof(detect_envelope_config_fields) /
                                    sizeof(detect_envelope_config_fields[0]),
                                detect_envelope_state_fields,
                                sizeof(detect_envelope_state_fields) / sizeof(detect_envelope_state_fields[0])
                            )
                                ENTRY_WITH_FIELDS(
                                    detect_peak,
                                    detect_peak_config_fields,
                                    sizeof(detect_peak_config_fields) /
                                        sizeof(detect_peak_config_fields[0]),
                                    detect_peak_state_fields,
                                    sizeof(detect_peak_state_fields) / sizeof(detect_peak_state_fields[0])
                                )
                                    ENTRY_WITH_FIELDS(
                                        detect_rms,
                                        detect_rms_config_fields,
                                        sizeof(detect_rms_config_fields) / sizeof(detect_rms_config_fields[0]),
                                        detect_rms_state_fields,
                                        sizeof(detect_rms_state_fields) /
                                            sizeof(detect_rms_state_fields[0])
                                    ) ENTRY_WITH_FIELDS(detect_slope, NULL, 0, detect_slope_state_fields, sizeof(detect_slope_state_fields) / sizeof(detect_slope_state_fields[0]))
                                        ENTRY_WITH_FIELDS(
                                            detect_threshold,
                                            detect_threshold_config_fields,
                                            sizeof(detect_threshold_config_fields) /
                                                sizeof(detect_threshold_config_fields[0]),
                                            NULL,
                                            0
                                        ) ENTRY_WITH_FIELDS(detect_zero_crossing, NULL, 0, detect_zero_crossing_state_fields, sizeof(detect_zero_crossing_state_fields) / sizeof(detect_zero_crossing_state_fields[0])) ENTRY_WITH_FIELDS(filter_allpass, filter_allpass_config_fields, sizeof(filter_allpass_config_fields) / sizeof(filter_allpass_config_fields[0]), filter_allpass_state_fields, sizeof(filter_allpass_state_fields) / sizeof(filter_allpass_state_fields[0]))
                                            ENTRY_WITH_FIELDS(filter_biquad, filter_biquad_config_fields, sizeof(filter_biquad_config_fields) / sizeof(filter_biquad_config_fields[0]), filter_biquad_state_fields, sizeof(filter_biquad_state_fields) / sizeof(filter_biquad_state_fields[0])) ENTRY_WITH_FIELDS(
                                                filter_comb_fb,
                                                filter_comb_fb_config_fields,
                                                sizeof(filter_comb_fb_config_fields) /
                                                    sizeof(filter_comb_fb_config_fields[0]),
                                                filter_comb_fb_state_fields,
                                                sizeof(filter_comb_fb_state_fields) /
                                                    sizeof(filter_comb_fb_state_fields[0])
                                            )
                                                ENTRY_WITH_FIELDS(
                                                    filter_comb_ff,
                                                    filter_comb_ff_config_fields,
                                                    sizeof(filter_comb_ff_config_fields) /
                                                        sizeof(filter_comb_ff_config_fields[0]),
                                                    filter_comb_ff_state_fields,
                                                    sizeof(filter_comb_ff_state_fields) /
                                                        sizeof(filter_comb_ff_state_fields[0])
                                                ) ENTRY_WITH_FIELDS(filter_dc_block, filter_dc_block_config_fields, sizeof(filter_dc_block_config_fields) / sizeof(filter_dc_block_config_fields[0]), filter_dc_block_state_fields, sizeof(filter_dc_block_state_fields) / sizeof(filter_dc_block_state_fields[0])) ENTRY_WITH_FIELDS(filter_differentiate, NULL, 0, filter_differentiate_state_fields, sizeof(filter_differentiate_state_fields) / sizeof(filter_differentiate_state_fields[0]))
                                                    ENTRY_WITH_FIELDS(filter_fir, filter_fir_config_fields, sizeof(filter_fir_config_fields) / sizeof(filter_fir_config_fields[0]), filter_fir_state_fields, sizeof(filter_fir_state_fields) / sizeof(filter_fir_state_fields[0])) ENTRY_WITH_FIELDS(filter_integrate, NULL, 0, filter_integrate_state_fields, sizeof(filter_integrate_state_fields) / sizeof(filter_integrate_state_fields[0])) ENTRY_WITH_FIELDS(
                                                        freq_fft,
                                                        freq_fft_config_fields,
                                                        sizeof(freq_fft_config_fields) /
                                                            sizeof(freq_fft_config_fields[0]),
                                                        NULL,
                                                        0
                                                    ) ENTRY_WITH_FIELDS(freq_ifft, freq_ifft_config_fields, sizeof(freq_ifft_config_fields) / sizeof(freq_ifft_config_fields[0]), NULL, 0) ENTRY_WITH_FIELDS(freq_multiply, freq_multiply_config_fields, sizeof(freq_multiply_config_fields) / sizeof(freq_multiply_config_fields[0]), NULL, 0) ENTRY_WITH_FIELDS(freq_overlap_add, freq_overlap_add_config_fields, sizeof(freq_overlap_add_config_fields) / sizeof(freq_overlap_add_config_fields[0]), freq_overlap_add_state_fields, sizeof(freq_overlap_add_state_fields) / sizeof(freq_overlap_add_state_fields[0]))
                                                        ENTRY_WITH_FIELDS(
                                                            freq_overlap_save,
                                                            freq_overlap_save_config_fields,
                                                            sizeof(freq_overlap_save_config_fields) /
                                                                sizeof(freq_overlap_save_config_fields[0]),
                                                            freq_overlap_save_state_fields,
                                                            sizeof(freq_overlap_save_state_fields) /
                                                                sizeof(freq_overlap_save_state_fields[0])
                                                        )
                                                            ENTRY_WITH_FIELDS(freq_overlap_add, freq_overlap_add_config_fields, sizeof(freq_overlap_add_config_fields) / sizeof(freq_overlap_add_config_fields[0]), freq_overlap_add_state_fields, sizeof(freq_overlap_add_state_fields) / sizeof(freq_overlap_add_state_fields[0])) ENTRY_WITH_FIELDS(
                                                                freq_overlap_save,
                                                                freq_overlap_save_config_fields,
                                                                sizeof(freq_overlap_save_config_fields) /
                                                                    sizeof(freq_overlap_save_config_fields[0]),
                                                                freq_overlap_save_state_fields,
                                                                sizeof(freq_overlap_save_state_fields) /
                                                                    sizeof(freq_overlap_save_state_fields[0])
                                                            ) ENTRY_WITH_FIELDS(freq_window, freq_window_config_fields, sizeof(freq_window_config_fields) / sizeof(freq_window_config_fields[0]), NULL, 0)
                                                                ENTRY_WITH_FIELDS(
                                                                    freq_shift,
                                                                    freq_shift_config_fields,
                                                                    sizeof(freq_shift_config_fields) /
                                                                        sizeof(freq_shift_config_fields[0]),
                                                                    freq_shift_state_fields,
                                                                    sizeof(freq_shift_state_fields) /
                                                                        sizeof(freq_shift_state_fields[0])
                                                                )

                                                                    ENTRY_WITH_FIELDS(
                                                                        generation_dc,
                                                                        generation_dc_config_fields,
                                                                        sizeof(generation_dc_config_fields) /
                                                                            sizeof(generation_dc_config_fields[0]),
                                                                        NULL,
                                                                        0
                                                                    )
                                                                        ENTRY_WITH_FIELDS(
                                                                            generation_envelope,
                                                                            generation_envelope_config_fields,
                                                                            sizeof(generation_envelope_config_fields) /
                                                                                sizeof(generation_envelope_config_fields
                                                                                           [0]
                                                                                ),
                                                                            generation_envelope_state_fields,
                                                                            sizeof(generation_envelope_state_fields) /
                                                                                sizeof(generation_envelope_state_fields
                                                                                           [0]
                                                                                )
                                                                        ) ENTRY_WITH_FIELDS(generation_impulse, generation_impulse_config_fields, sizeof(generation_impulse_config_fields) / sizeof(generation_impulse_config_fields[0]), generation_impulse_state_fields, sizeof(generation_impulse_state_fields) / sizeof(generation_impulse_state_fields[0])) ENTRY_WITH_FIELDS(generation_lfo, generation_lfo_config_fields, sizeof(generation_lfo_config_fields) / sizeof(generation_lfo_config_fields[0]), generation_lfo_state_fields, sizeof(generation_lfo_state_fields) / sizeof(generation_lfo_state_fields[0]))
                                                                            ENTRY_WITH_FIELDS(
                                                                                generation_noise,
                                                                                generation_noise_config_fields,
                                                                                sizeof(generation_noise_config_fields) /
                                                                                    sizeof(
                                                                                        generation_noise_config_fields
                                                                                            [0]
                                                                                    ),
                                                                                generation_noise_state_fields,
                                                                                sizeof(generation_noise_state_fields) /
                                                                                    sizeof(generation_noise_state_fields
                                                                                               [0])
                                                                            )
                                                                                ENTRY_WITH_FIELDS(
                                                                                    generation_oscillator,
                                                                                    generation_oscillator_config_fields,
                                                                                    sizeof(
                                                                                        generation_oscillator_config_fields
                                                                                    ) /
                                                                                        sizeof(
                                                                                            generation_oscillator_config_fields
                                                                                                [0]
                                                                                        ),
                                                                                    generation_oscillator_state_fields,
                                                                                    sizeof(
                                                                                        generation_oscillator_state_fields
                                                                                    ) /
                                                                                        sizeof(
                                                                                            generation_oscillator_state_fields
                                                                                                [0]
                                                                                        )
                                                                                ) ENTRY_WITH_FIELDS(interpolation_cubic, NULL, 0, NULL, 0)
                                                                                    ENTRY_WITH_FIELDS(
                                                                                        interpolation_lagrange,
                                                                                        interpolation_lagrange_config_fields,
                                                                                        sizeof(
                                                                                            interpolation_lagrange_config_fields
                                                                                        ) /
                                                                                            sizeof(
                                                                                                interpolation_lagrange_config_fields
                                                                                                    [0]
                                                                                            ),
                                                                                        interpolation_lagrange_state_fields,
                                                                                        sizeof(
                                                                                            interpolation_lagrange_state_fields
                                                                                        ) /
                                                                                            sizeof(
                                                                                                interpolation_lagrange_state_fields
                                                                                                    [0]
                                                                                            )
                                                                                    ) ENTRY_WITH_FIELDS(interpolation_linear, NULL, 0, NULL, 0)
                                                                                        ENTRY_WITH_FIELDS(interpolation_sinc, interpolation_sinc_config_fields, sizeof(interpolation_sinc_config_fields) / sizeof(interpolation_sinc_config_fields[0]), interpolation_sinc_state_fields, sizeof(interpolation_sinc_state_fields) / sizeof(interpolation_sinc_state_fields[0])) ENTRY_WITH_FIELDS(mix_crossfade, mix_crossfade_config_fields, sizeof(mix_crossfade_config_fields) / sizeof(mix_crossfade_config_fields[0]), NULL, 0) ENTRY_WITH_FIELDS(mix_decode_ms, NULL, 0, NULL, 0) ENTRY_WITH_FIELDS(mix_encode_ms, NULL, 0, NULL, 0) ENTRY_WITH_FIELDS(
                                                                                            mix_matrix,
                                                                                            mix_matrix_config_fields,
                                                                                            sizeof(
                                                                                                mix_matrix_config_fields
                                                                                            ) /
                                                                                                sizeof(
                                                                                                    mix_matrix_config_fields
                                                                                                        [0]
                                                                                                ),
                                                                                            NULL,
                                                                                            0
                                                                                        )
                                                                                            ENTRY_WITH_FIELDS(
                                                                                                mix_pan_stereo,
                                                                                                mix_pan_stereo_config_fields,
                                                                                                sizeof(
                                                                                                    mix_pan_stereo_config_fields
                                                                                                ) /
                                                                                                    sizeof(
                                                                                                        mix_pan_stereo_config_fields
                                                                                                            [0]
                                                                                                    ),
                                                                                                NULL,
                                                                                                0
                                                                                            ) ENTRY_WITH_FIELDS(mix_wet_dry, mix_wet_dry_config_fields, sizeof(mix_wet_dry_config_fields) / sizeof(mix_wet_dry_config_fields[0]), NULL, 0) ENTRY_WITH_FIELDS(modulation_amplitude, modulation_amplitude_config_fields, sizeof(modulation_amplitude_config_fields) / sizeof(modulation_amplitude_config_fields[0]), NULL, 0) ENTRY_WITH_FIELDS(modulation_frequency, modulation_frequency_config_fields, sizeof(modulation_frequency_config_fields) / sizeof(modulation_frequency_config_fields[0]), modulation_frequency_state_fields, sizeof(modulation_frequency_state_fields) / sizeof(modulation_frequency_state_fields[0]))
                                                                                                ENTRY_WITH_FIELDS(
                                                                                                    modulation_phase,
                                                                                                    modulation_phase_config_fields,
                                                                                                    sizeof(
                                                                                                        modulation_phase_config_fields
                                                                                                    ) /
                                                                                                        sizeof(
                                                                                                            modulation_phase_config_fields
                                                                                                                [0]
                                                                                                        ),
                                                                                                    modulation_phase_state_fields,
                                                                                                    sizeof(
                                                                                                        modulation_phase_state_fields
                                                                                                    ) /
                                                                                                        sizeof(
                                                                                                            modulation_phase_state_fields
                                                                                                                [0]
                                                                                                        )
                                                                                                ) ENTRY_WITH_FIELDS(modulation_ring, NULL, 0, NULL, 0)
                                                                                                    ENTRY_WITH_FIELDS(
                                                                                                        modulation_scrub,
                                                                                                        modulation_scrub_config_fields,
                                                                                                        sizeof(
                                                                                                            modulation_scrub_config_fields
                                                                                                        ) /
                                                                                                            sizeof(
                                                                                                                modulation_scrub_config_fields
                                                                                                                    [0]
                                                                                                            ),
                                                                                                        NULL,
                                                                                                        0
                                                                                                    )
                                                                                                        ENTRY_WITH_FIELDS(
                                                                                                            nonlinear_bitcrush,
                                                                                                            nonlinear_bitcrush_config_fields,
                                                                                                            sizeof(
                                                                                                                nonlinear_bitcrush_config_fields
                                                                                                            ) /
                                                                                                                sizeof(
                                                                                                                    nonlinear_bitcrush_config_fields
                                                                                                                        [0]
                                                                                                                ),
                                                                                                            NULL,
                                                                                                            0
                                                                                                        ) ENTRY_WITH_FIELDS(nonlinear_samplerate_reduce, nonlinear_samplerate_reduce_config_fields, sizeof(nonlinear_samplerate_reduce_config_fields) / sizeof(nonlinear_samplerate_reduce_config_fields[0]), nonlinear_samplerate_reduce_state_fields, sizeof(nonlinear_samplerate_reduce_state_fields) / sizeof(nonlinear_samplerate_reduce_state_fields[0])) ENTRY_WITH_FIELDS(nonlinear_waveshape, nonlinear_waveshape_config_fields, sizeof(nonlinear_waveshape_config_fields) / sizeof(nonlinear_waveshape_config_fields[0]), NULL, 0)
                                                                                                            ENTRY_WITH_FIELDS(
                                                                                                                src_antialias,
                                                                                                                src_antialias_config_fields,
                                                                                                                sizeof(
                                                                                                                    src_antialias_config_fields
                                                                                                                ) /
                                                                                                                    sizeof(
                                                                                                                        src_antialias_config_fields
                                                                                                                            [0]
                                                                                                                    ),
                                                                                                                src_antialias_state_fields,
                                                                                                                sizeof(
                                                                                                                    src_antialias_state_fields
                                                                                                                ) /
                                                                                                                    sizeof(
                                                                                                                        src_antialias_state_fields
                                                                                                                            [0]
                                                                                                                    )
                                                                                                            ) ENTRY_WITH_FIELDS(src_antiimage, src_antiimage_config_fields, sizeof(src_antiimage_config_fields) / sizeof(src_antiimage_config_fields[0]), src_antiimage_state_fields, sizeof(src_antiimage_state_fields) / sizeof(src_antiimage_state_fields[0]))
                                                                                                                ENTRY_WITH_FIELDS(
                                                                                                                    src_convert_format,
                                                                                                                    src_convert_format_config_fields,
                                                                                                                    sizeof(
                                                                                                                        src_convert_format_config_fields
                                                                                                                    ) /
                                                                                                                        sizeof(
                                                                                                                            src_convert_format_config_fields
                                                                                                                                [0]
                                                                                                                        ),
                                                                                                                    NULL,
                                                                                                                    0
                                                                                                                )
                                                                                                                    ENTRY_WITH_FIELDS(
                                                                                                                        src_downsample,
                                                                                                                        src_downsample_config_fields,
                                                                                                                        sizeof(
                                                                                                                            src_downsample_config_fields
                                                                                                                        ) /
                                                                                                                            sizeof(
                                                                                                                                src_downsample_config_fields
                                                                                                                                    [0]
                                                                                                                            ),
                                                                                                                        NULL,
                                                                                                                        0
                                                                                                                    )
                                                                                                                        ENTRY_WITH_FIELDS(
                                                                                                                            src_upsample,
                                                                                                                            src_upsample_config_fields,
                                                                                                                            sizeof(
                                                                                                                                src_upsample_config_fields
                                                                                                                            ) /
                                                                                                                                sizeof(
                                                                                                                                    src_upsample_config_fields
                                                                                                                                        [0]
                                                                                                                                ),
                                                                                                                            NULL,
                                                                                                                            0
                                                                                                                        )
                                                                                                                            ENTRY_WITH_FIELDS(
                                                                                                                                freq_quantize,
                                                                                                                                NULL,
                                                                                                                                0,
                                                                                                                                NULL,
                                                                                                                                0
                                                                                                                            )
};

static const int g_registry_count = sizeof(g_registry) / sizeof(g_registry[0]);

void atom_registry_init(void) {}

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
