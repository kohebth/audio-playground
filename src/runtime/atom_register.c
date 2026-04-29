#include "atom/dsp_atoms.h"
#include <atom_registry.h>
#include <string.h>

// ─────────────────────────────────────────────
// Thunk generation
// ─────────────────────────────────────────────

#define THUNK(_) \
    static void _##_thunk(atom_call_t *call) { \
        _((_##_out_t *)call->out, (_##_in_t *)call->in, \
          (_##_params_t *)call->config, (_##_state_t *)call->state); \
    }

#define REGISTER_ALL(_) \
    _(amplitude_accumulate) \
    _(amplitude_add) \
    _(amplitude_clip_hard) \
    _(amplitude_clip_soft) \
    _(amplitude_divide) \
    _(amplitude_multiply) \
    _(amplitude_normalize) \
    _(amplitude_smooth) \
    _(amplitude_subtract) \
    _(delay_fractional) \
    _(delay_line) \
    _(delay_tap_feedback) \
    _(delay_tap_feedforward) \
    _(delay_unit) \
    _(detect_autocorrelate) \
    _(detect_envelope) \
    _(detect_peak) \
    _(detect_rms) \
    _(detect_slope) \
    _(detect_threshold) \
    _(detect_zero_crossing) \
    _(filter_allpass) \
    _(filter_biquad) \
    _(filter_comb_fb) \
    _(filter_comb_ff) \
    _(filter_dc_block) \
    _(filter_differentiate) \
    _(filter_fir) \
    _(filter_integrate) \
    _(freq_fft) \
    _(freq_ifft) \
    _(freq_multiply) \
    _(freq_overlap_add) \
    _(freq_overlap_save) \
    _(freq_window) \
    _(generation_dc) \
    _(generation_envelope) \
    _(generation_impulse) \
    _(generation_lfo) \
    _(generation_noise) \
    _(generation_oscillator) \
    _(interpolation_cubic) \
    _(interpolation_lagrange) \
    _(interpolation_linear) \
    _(interpolation_sinc) \
    _(mix_crossfade) \
    _(mix_decode_ms) \
    _(mix_encode_ms) \
    _(mix_matrix) \
    _(mix_pan_stereo) \
    _(mix_wet_dry) \
    _(modulation_amplitude) \
    _(modulation_frequency) \
    _(modulation_phase) \
    _(modulation_ring) \
    _(modulation_scrub) \
    _(nonlinear_bitcrush) \
    _(nonlinear_samplerate_reduce) \
    _(nonlinear_waveshape) \
    _(src_antialias) \
    _(src_antiimage) \
    _(src_convert_format) \
    _(src_downsample) \
    _(src_upsample) \
    /* end */

REGISTER_ALL(THUNK)

// ─────────────────────────────────────────────
// Field Descriptors
// ─────────────────────────────────────────────

static const atom_field_desc_t amplitude_accumulate_state_fields[] = {
    { "accumulator", FIELD_FLOAT, offsetof(amplitude_accumulate_state_t, accumulator) },
};

static const atom_field_desc_t amplitude_clip_hard_config_fields[] = {
    { "threshold", FIELD_FLOAT, offsetof(amplitude_clip_hard_params_t, threshold) },
};

static const atom_field_desc_t amplitude_clip_soft_config_fields[] = {
    { "threshold", FIELD_FLOAT, offsetof(amplitude_clip_soft_params_t, threshold) },
    { "curve", FIELD_INT, offsetof(amplitude_clip_soft_params_t, curve) },
};

static const atom_field_desc_t amplitude_divide_config_fields[] = {
    { "epsilon", FIELD_FLOAT, offsetof(amplitude_divide_params_t, epsilon) },
};

static const atom_field_desc_t amplitude_normalize_config_fields[] = {
    { "target_level", FIELD_FLOAT, offsetof(amplitude_normalize_params_t, target_level) },
    { "mode", FIELD_INT, offsetof(amplitude_normalize_params_t, mode) },
};

static const atom_field_desc_t amplitude_normalize_state_fields[] = {
    { "running_peak", FIELD_FLOAT, offsetof(amplitude_normalize_state_t, running_peak) },
};

static const atom_field_desc_t amplitude_smooth_config_fields[] = {
    { "attack", FIELD_FLOAT, offsetof(amplitude_smooth_params_t, attack) },
    { "release", FIELD_FLOAT, offsetof(amplitude_smooth_params_t, release) },
    { "sample_rate", FIELD_FLOAT, offsetof(amplitude_smooth_params_t, sample_rate) },
};

static const atom_field_desc_t amplitude_smooth_state_fields[] = {
    { "prev_value", FIELD_FLOAT, offsetof(amplitude_smooth_state_t, prev_value) },
};

static const atom_field_desc_t delay_fractional_config_fields[] = {
    { "delay_samples", FIELD_FLOAT, offsetof(delay_fractional_params_t, delay_samples) },
    { "interpolation", FIELD_INT, offsetof(delay_fractional_params_t, interpolation) },
};

static const atom_field_desc_t delay_fractional_state_fields[] = {
    { "buffer", FIELD_BUFFER, offsetof(delay_fractional_state_t, buffer) },
    { "write_pos", FIELD_INT, offsetof(delay_fractional_state_t, write_pos) },
};

static const atom_field_desc_t delay_line_config_fields[] = {
    { "length", FIELD_INT, offsetof(delay_line_params_t, length) },
};

static const atom_field_desc_t delay_line_state_fields[] = {
    { "buffer", FIELD_BUFFER, offsetof(delay_line_state_t, buffer) },
    { "write_pos", FIELD_INT, offsetof(delay_line_state_t, write_pos) },
};

static const atom_field_desc_t delay_tap_feedback_config_fields[] = {
    { "coefficient", FIELD_FLOAT, offsetof(delay_tap_feedback_params_t, coefficient) },
};

static const atom_field_desc_t delay_tap_feedforward_config_fields[] = {
    { "coefficient", FIELD_FLOAT, offsetof(delay_tap_feedforward_params_t, coefficient) },
};

static const atom_field_desc_t delay_unit_state_fields[] = {
    { "prev_sample", FIELD_FLOAT, offsetof(delay_unit_state_t, prev_sample) },
};

static const atom_field_desc_t detect_autocorrelate_config_fields[] = {
    { "max_lag", FIELD_INT, offsetof(detect_autocorrelate_params_t, max_lag) },
};

static const atom_field_desc_t detect_autocorrelate_state_fields[] = {
    { "buffer", FIELD_BUFFER, offsetof(detect_autocorrelate_state_t, buffer) },
    { "write_pos", FIELD_INT, offsetof(detect_autocorrelate_state_t, write_pos) },
};

static const atom_field_desc_t detect_envelope_config_fields[] = {
    { "attack", FIELD_FLOAT, offsetof(detect_envelope_params_t, attack) },
    { "release", FIELD_FLOAT, offsetof(detect_envelope_params_t, release) },
    { "sample_rate", FIELD_FLOAT, offsetof(detect_envelope_params_t, sample_rate) },
};

static const atom_field_desc_t detect_envelope_state_fields[] = {
    { "prev_envelope", FIELD_FLOAT, offsetof(detect_envelope_state_t, prev_envelope) },
};

static const atom_field_desc_t detect_peak_config_fields[] = {
    { "attack", FIELD_FLOAT, offsetof(detect_peak_params_t, attack) },
    { "release", FIELD_FLOAT, offsetof(detect_peak_params_t, release) },
    { "sample_rate", FIELD_FLOAT, offsetof(detect_peak_params_t, sample_rate) },
};

static const atom_field_desc_t detect_peak_state_fields[] = {
    { "prev_peak", FIELD_FLOAT, offsetof(detect_peak_state_t, prev_peak) },
};

static const atom_field_desc_t detect_rms_config_fields[] = {
    { "window_size", FIELD_INT, offsetof(detect_rms_params_t, window_size) },
};

static const atom_field_desc_t detect_rms_state_fields[] = {
    { "buffer", FIELD_BUFFER, offsetof(detect_rms_state_t, buffer) },
    { "write_pos", FIELD_INT, offsetof(detect_rms_state_t, write_pos) },
    { "sum", FIELD_FLOAT, offsetof(detect_rms_state_t, sum) },
};

static const atom_field_desc_t detect_slope_state_fields[] = {
    { "prev_sample", FIELD_FLOAT, offsetof(detect_slope_state_t, prev_sample) },
};

static const atom_field_desc_t detect_threshold_config_fields[] = {
    { "threshold", FIELD_FLOAT, offsetof(detect_threshold_params_t, threshold) },
};

static const atom_field_desc_t detect_zero_crossing_state_fields[] = {
    { "prev_sample", FIELD_FLOAT, offsetof(detect_zero_crossing_state_t, prev_sample) },
};

static const atom_field_desc_t filter_allpass_config_fields[] = {
    { "delay_samples", FIELD_INT, offsetof(filter_allpass_params_t, delay_samples) },
    { "coefficient", FIELD_FLOAT, offsetof(filter_allpass_params_t, coefficient) },
};

static const atom_field_desc_t filter_allpass_state_fields[] = {
    { "buffer", FIELD_BUFFER, offsetof(filter_allpass_state_t, buffer) },
    { "write_pos", FIELD_INT, offsetof(filter_allpass_state_t, write_pos) },
};

static const atom_field_desc_t filter_biquad_config_fields[] = {
    { "b0", FIELD_FLOAT, offsetof(filter_biquad_params_t, b0) },
    { "b1", FIELD_FLOAT, offsetof(filter_biquad_params_t, b1) },
    { "b2", FIELD_FLOAT, offsetof(filter_biquad_params_t, b2) },
    { "a1", FIELD_FLOAT, offsetof(filter_biquad_params_t, a1) },
    { "a2", FIELD_FLOAT, offsetof(filter_biquad_params_t, a2) },
};

static const atom_field_desc_t filter_biquad_state_fields[] = {
    { "z1", FIELD_FLOAT, offsetof(filter_biquad_state_t, z1) },
    { "z2", FIELD_FLOAT, offsetof(filter_biquad_state_t, z2) },
};

static const atom_field_desc_t filter_comb_fb_config_fields[] = {
    { "delay_samples", FIELD_INT, offsetof(filter_comb_fb_params_t, delay_samples) },
    { "coefficient", FIELD_FLOAT, offsetof(filter_comb_fb_params_t, coefficient) },
};

static const atom_field_desc_t filter_comb_fb_state_fields[] = {
    { "buffer", FIELD_BUFFER, offsetof(filter_comb_fb_state_t, buffer) },
    { "write_pos", FIELD_INT, offsetof(filter_comb_fb_state_t, write_pos) },
};

static const atom_field_desc_t filter_comb_ff_config_fields[] = {
    { "delay_samples", FIELD_INT, offsetof(filter_comb_ff_params_t, delay_samples) },
    { "coefficient", FIELD_FLOAT, offsetof(filter_comb_ff_params_t, coefficient) },
};

static const atom_field_desc_t filter_comb_ff_state_fields[] = {
    { "buffer", FIELD_BUFFER, offsetof(filter_comb_ff_state_t, buffer) },
    { "write_pos", FIELD_INT, offsetof(filter_comb_ff_state_t, write_pos) },
};

static const atom_field_desc_t filter_dc_block_config_fields[] = {
    { "coefficient", FIELD_FLOAT, offsetof(filter_dc_block_params_t, coefficient) },
};

static const atom_field_desc_t filter_dc_block_state_fields[] = {
    { "prev_input", FIELD_FLOAT, offsetof(filter_dc_block_state_t, prev_input) },
    { "prev_output", FIELD_FLOAT, offsetof(filter_dc_block_state_t, prev_output) },
};

static const atom_field_desc_t filter_differentiate_state_fields[] = {
    { "prev_sample", FIELD_FLOAT, offsetof(filter_differentiate_state_t, prev_sample) },
};

static const atom_field_desc_t filter_fir_config_fields[] = {
    { "kernel", FIELD_BUFFER, offsetof(filter_fir_params_t, kernel) },
    { "kernel_size", FIELD_INT, offsetof(filter_fir_params_t, kernel_size) },
};

static const atom_field_desc_t filter_fir_state_fields[] = {
    { "buffer", FIELD_BUFFER, offsetof(filter_fir_state_t, buffer) },
    { "write_pos", FIELD_INT, offsetof(filter_fir_state_t, write_pos) },
};

static const atom_field_desc_t filter_integrate_state_fields[] = {
    { "accumulator", FIELD_FLOAT, offsetof(filter_integrate_state_t, accumulator) },
};

static const atom_field_desc_t freq_fft_config_fields[] = {
    { "block_size", FIELD_INT, offsetof(freq_fft_params_t, block_size) },
};

static const atom_field_desc_t freq_ifft_config_fields[] = {
    { "block_size", FIELD_INT, offsetof(freq_ifft_params_t, block_size) },
};

static const atom_field_desc_t freq_multiply_config_fields[] = {
    { "block_size", FIELD_INT, offsetof(freq_multiply_params_t, block_size) },
};

static const atom_field_desc_t freq_overlap_add_config_fields[] = {
    { "block_size", FIELD_INT, offsetof(freq_overlap_add_params_t, block_size) },
    { "hop_size", FIELD_INT, offsetof(freq_overlap_add_params_t, hop_size) },
};

static const atom_field_desc_t freq_overlap_add_state_fields[] = {
    { "buffer", FIELD_BUFFER, offsetof(freq_overlap_add_state_t, buffer) },
};

static const atom_field_desc_t freq_overlap_save_config_fields[] = {
    { "block_size", FIELD_INT, offsetof(freq_overlap_save_params_t, block_size) },
    { "hop_size", FIELD_INT, offsetof(freq_overlap_save_params_t, hop_size) },
};

static const atom_field_desc_t freq_overlap_save_state_fields[] = {
    { "buffer", FIELD_BUFFER, offsetof(freq_overlap_save_state_t, buffer) },
    { "write_pos", FIELD_INT, offsetof(freq_overlap_save_state_t, write_pos) },
};

static const atom_field_desc_t freq_window_config_fields[] = {
    { "window_type", FIELD_INT, offsetof(freq_window_params_t, window_type) },
    { "block_size", FIELD_INT, offsetof(freq_window_params_t, block_size) },
};

static const atom_field_desc_t generation_dc_config_fields[] = {
    { "value", FIELD_FLOAT, offsetof(generation_dc_params_t, value) },
};

static const atom_field_desc_t generation_envelope_config_fields[] = {
    { "attack", FIELD_FLOAT, offsetof(generation_envelope_params_t, attack) },
    { "decay", FIELD_FLOAT, offsetof(generation_envelope_params_t, decay) },
    { "sustain", FIELD_FLOAT, offsetof(generation_envelope_params_t, sustain) },
    { "release", FIELD_FLOAT, offsetof(generation_envelope_params_t, release) },
    { "sample_rate", FIELD_FLOAT, offsetof(generation_envelope_params_t, sample_rate) },
};

static const atom_field_desc_t generation_envelope_state_fields[] = {
    { "current_level", FIELD_FLOAT, offsetof(generation_envelope_state_t, current_level) },
    { "stage", FIELD_INT, offsetof(generation_envelope_state_t, stage) },
};

static const atom_field_desc_t generation_impulse_config_fields[] = {
    { "interval", FIELD_FLOAT, offsetof(generation_impulse_params_t, interval) },
    { "sample_rate", FIELD_FLOAT, offsetof(generation_impulse_params_t, sample_rate) },
};

static const atom_field_desc_t generation_impulse_state_fields[] = {
    { "counter", FIELD_INT, offsetof(generation_impulse_state_t, counter) },
};

static const atom_field_desc_t generation_lfo_config_fields[] = {
    { "frequency", FIELD_FLOAT, offsetof(generation_lfo_params_t, frequency) },
    { "waveform", FIELD_INT, offsetof(generation_lfo_params_t, waveform) },
    { "phase_offset", FIELD_FLOAT, offsetof(generation_lfo_params_t, phase_offset) },
    { "sample_rate", FIELD_FLOAT, offsetof(generation_lfo_params_t, sample_rate) },
};

static const atom_field_desc_t generation_lfo_state_fields[] = {
    { "phase", FIELD_FLOAT, offsetof(generation_lfo_state_t, phase) },
};

static const atom_field_desc_t generation_noise_config_fields[] = {
    { "amplitude", FIELD_FLOAT, offsetof(generation_noise_params_t, amplitude) },
    { "color", FIELD_INT, offsetof(generation_noise_params_t, color) },
};

static const atom_field_desc_t generation_noise_state_fields[] = {
    { "seed", FIELD_INT, offsetof(generation_noise_state_t, seed) },
    { "prev_value", FIELD_FLOAT, offsetof(generation_noise_state_t, prev_value) },
};

static const atom_field_desc_t generation_oscillator_config_fields[] = {
    { "frequency", FIELD_FLOAT, offsetof(generation_oscillator_params_t, frequency) },
    { "waveform", FIELD_INT, offsetof(generation_oscillator_params_t, waveform) },
    { "phase_offset", FIELD_FLOAT, offsetof(generation_oscillator_params_t, phase_offset) },
    { "sample_rate", FIELD_FLOAT, offsetof(generation_oscillator_params_t, sample_rate) },
};

static const atom_field_desc_t generation_oscillator_state_fields[] = {
    { "phase", FIELD_FLOAT, offsetof(generation_oscillator_state_t, phase) },
};

static const atom_field_desc_t interpolation_lagrange_config_fields[] = {
    { "order", FIELD_INT, offsetof(interpolation_lagrange_params_t, order) },
};

static const atom_field_desc_t interpolation_lagrange_state_fields[] = {
    { "buffer", FIELD_BUFFER, offsetof(interpolation_lagrange_state_t, buffer) },
    { "write_pos", FIELD_INT, offsetof(interpolation_lagrange_state_t, write_pos) },
};

static const atom_field_desc_t interpolation_sinc_config_fields[] = {
    { "num_taps", FIELD_INT, offsetof(interpolation_sinc_params_t, num_taps) },
};

static const atom_field_desc_t interpolation_sinc_state_fields[] = {
    { "taps", FIELD_BUFFER, offsetof(interpolation_sinc_state_t, taps) },
};

static const atom_field_desc_t mix_crossfade_config_fields[] = {
    { "t", FIELD_FLOAT, offsetof(mix_crossfade_params_t, t) },
};

static const atom_field_desc_t mix_matrix_config_fields[] = {
    { "num_in", FIELD_INT, offsetof(mix_matrix_params_t, num_in) },
    { "num_out", FIELD_INT, offsetof(mix_matrix_params_t, num_out) },
};

static const atom_field_desc_t mix_pan_stereo_config_fields[] = {
    { "position", FIELD_FLOAT, offsetof(mix_pan_stereo_params_t, position) },
};

static const atom_field_desc_t mix_wet_dry_config_fields[] = {
    { "mix", FIELD_FLOAT, offsetof(mix_wet_dry_params_t, mix) },
};

static const atom_field_desc_t modulation_amplitude_config_fields[] = {
    { "depth", FIELD_FLOAT, offsetof(modulation_amplitude_params_t, depth) },
};

static const atom_field_desc_t modulation_frequency_config_fields[] = {
    { "depth", FIELD_FLOAT, offsetof(modulation_frequency_params_t, depth) },
};

static const atom_field_desc_t modulation_frequency_state_fields[] = {
    { "buffer", FIELD_BUFFER, offsetof(modulation_frequency_state_t, buffer) },
    { "write_pos", FIELD_INT, offsetof(modulation_frequency_state_t, write_pos) },
    { "current_delay", FIELD_FLOAT, offsetof(modulation_frequency_state_t, current_delay) },
};

static const atom_field_desc_t modulation_phase_config_fields[] = {
    { "depth", FIELD_FLOAT, offsetof(modulation_phase_params_t, depth) },
};

static const atom_field_desc_t modulation_phase_state_fields[] = {
    { "buffer", FIELD_BUFFER, offsetof(modulation_phase_state_t, buffer) },
    { "write_pos", FIELD_INT, offsetof(modulation_phase_state_t, write_pos) },
};

static const atom_field_desc_t modulation_scrub_config_fields[] = {
    { "buffer_size", FIELD_INT, offsetof(modulation_scrub_params_t, buffer_size) },
};

static const atom_field_desc_t nonlinear_bitcrush_config_fields[] = {
    { "bit_depth", FIELD_FLOAT, offsetof(nonlinear_bitcrush_params_t, bit_depth) },
};

static const atom_field_desc_t nonlinear_samplerate_reduce_config_fields[] = {
    { "factor", FIELD_FLOAT, offsetof(nonlinear_samplerate_reduce_params_t, factor) },
};

static const atom_field_desc_t nonlinear_samplerate_reduce_state_fields[] = {
    { "last_val", FIELD_FLOAT, offsetof(nonlinear_samplerate_reduce_state_t, last_val) },
    { "counter", FIELD_FLOAT, offsetof(nonlinear_samplerate_reduce_state_t, counter) },
};

static const atom_field_desc_t nonlinear_waveshape_config_fields[] = {
    { "transfer_table", FIELD_BUFFER, offsetof(nonlinear_waveshape_params_t, transfer_table) },
    { "table_size", FIELD_INT, offsetof(nonlinear_waveshape_params_t, table_size) },
};

static const atom_field_desc_t src_antialias_config_fields[] = {
    { "cutoff", FIELD_FLOAT, offsetof(src_antialias_params_t, cutoff) },
    { "sample_rate", FIELD_FLOAT, offsetof(src_antialias_params_t, sample_rate) },
};

static const atom_field_desc_t src_antialias_state_fields[] = {
    { "z1", FIELD_FLOAT, offsetof(src_antialias_state_t, z1) },
    { "z2", FIELD_FLOAT, offsetof(src_antialias_state_t, z2) },
};

static const atom_field_desc_t src_antiimage_config_fields[] = {
    { "cutoff", FIELD_FLOAT, offsetof(src_antiimage_params_t, cutoff) },
    { "sample_rate", FIELD_FLOAT, offsetof(src_antiimage_params_t, sample_rate) },
};

static const atom_field_desc_t src_antiimage_state_fields[] = {
    { "z1", FIELD_FLOAT, offsetof(src_antiimage_state_t, z1) },
    { "z2", FIELD_FLOAT, offsetof(src_antiimage_state_t, z2) },
};

static const atom_field_desc_t src_convert_format_config_fields[] = {
    { "from_format", FIELD_INT, offsetof(src_convert_format_params_t, from_format) },
    { "to_format", FIELD_INT, offsetof(src_convert_format_params_t, to_format) },
};

static const atom_field_desc_t src_downsample_config_fields[] = {
    { "factor", FIELD_INT, offsetof(src_downsample_params_t, factor) },
};

static const atom_field_desc_t src_upsample_config_fields[] = {
    { "factor", FIELD_INT, offsetof(src_upsample_params_t, factor) },
};

// ─────────────────────────────────────────────
// Registry table
// ─────────────────────────────────────────────

#define ENTRY_WITH_FIELDS(atom_name, c_fields, n_cfg, s_fields, n_st) { \
    .name           = #atom_name, \
    .thunk          = atom_name##_thunk, \
    .out_size       = sizeof(atom_name##_out_t), \
    .in_size        = sizeof(atom_name##_in_t), \
    .config_size    = sizeof(atom_name##_params_t), \
    .state_size     = sizeof(atom_name##_state_t), \
    .config_fields  = c_fields, \
    .n_config_fields = n_cfg, \
    .state_fields   = s_fields, \
    .n_state_fields  = n_st, \
},

static atom_registry_entry_t g_registry[] = {
    ENTRY_WITH_FIELDS(amplitude_accumulate, NULL, 0, amplitude_accumulate_state_fields, sizeof(amplitude_accumulate_state_fields)/sizeof(amplitude_accumulate_state_fields[0]))
    ENTRY_WITH_FIELDS(amplitude_add, NULL, 0, NULL, 0)
    ENTRY_WITH_FIELDS(amplitude_clip_hard, amplitude_clip_hard_config_fields, sizeof(amplitude_clip_hard_config_fields)/sizeof(amplitude_clip_hard_config_fields[0]), NULL, 0)
    ENTRY_WITH_FIELDS(amplitude_clip_soft, amplitude_clip_soft_config_fields, sizeof(amplitude_clip_soft_config_fields)/sizeof(amplitude_clip_soft_config_fields[0]), NULL, 0)
    ENTRY_WITH_FIELDS(amplitude_divide, amplitude_divide_config_fields, sizeof(amplitude_divide_config_fields)/sizeof(amplitude_divide_config_fields[0]), NULL, 0)
    ENTRY_WITH_FIELDS(amplitude_multiply, NULL, 0, NULL, 0)
    ENTRY_WITH_FIELDS(amplitude_normalize, amplitude_normalize_config_fields, sizeof(amplitude_normalize_config_fields)/sizeof(amplitude_normalize_config_fields[0]), amplitude_normalize_state_fields, sizeof(amplitude_normalize_state_fields)/sizeof(amplitude_normalize_state_fields[0]))
    ENTRY_WITH_FIELDS(amplitude_smooth, amplitude_smooth_config_fields, sizeof(amplitude_smooth_config_fields)/sizeof(amplitude_smooth_config_fields[0]), amplitude_smooth_state_fields, sizeof(amplitude_smooth_state_fields)/sizeof(amplitude_smooth_state_fields[0]))
    ENTRY_WITH_FIELDS(amplitude_subtract, NULL, 0, NULL, 0)
    ENTRY_WITH_FIELDS(delay_fractional, delay_fractional_config_fields, sizeof(delay_fractional_config_fields)/sizeof(delay_fractional_config_fields[0]), delay_fractional_state_fields, sizeof(delay_fractional_state_fields)/sizeof(delay_fractional_state_fields[0]))
    ENTRY_WITH_FIELDS(delay_line, delay_line_config_fields, sizeof(delay_line_config_fields)/sizeof(delay_line_config_fields[0]), delay_line_state_fields, sizeof(delay_line_state_fields)/sizeof(delay_line_state_fields[0]))
    ENTRY_WITH_FIELDS(delay_tap_feedback, delay_tap_feedback_config_fields, sizeof(delay_tap_feedback_config_fields)/sizeof(delay_tap_feedback_config_fields[0]), NULL, 0)
    ENTRY_WITH_FIELDS(delay_tap_feedforward, delay_tap_feedforward_config_fields, sizeof(delay_tap_feedforward_config_fields)/sizeof(delay_tap_feedforward_config_fields[0]), NULL, 0)
    ENTRY_WITH_FIELDS(delay_unit, NULL, 0, delay_unit_state_fields, sizeof(delay_unit_state_fields)/sizeof(delay_unit_state_fields[0]))
    ENTRY_WITH_FIELDS(detect_autocorrelate, detect_autocorrelate_config_fields, sizeof(detect_autocorrelate_config_fields)/sizeof(detect_autocorrelate_config_fields[0]), detect_autocorrelate_state_fields, sizeof(detect_autocorrelate_state_fields)/sizeof(detect_autocorrelate_state_fields[0]))
    ENTRY_WITH_FIELDS(detect_envelope, detect_envelope_config_fields, sizeof(detect_envelope_config_fields)/sizeof(detect_envelope_config_fields[0]), detect_envelope_state_fields, sizeof(detect_envelope_state_fields)/sizeof(detect_envelope_state_fields[0]))
    ENTRY_WITH_FIELDS(detect_peak, detect_peak_config_fields, sizeof(detect_peak_config_fields)/sizeof(detect_peak_config_fields[0]), detect_peak_state_fields, sizeof(detect_peak_state_fields)/sizeof(detect_peak_state_fields[0]))
    ENTRY_WITH_FIELDS(detect_rms, detect_rms_config_fields, sizeof(detect_rms_config_fields)/sizeof(detect_rms_config_fields[0]), detect_rms_state_fields, sizeof(detect_rms_state_fields)/sizeof(detect_rms_state_fields[0]))
    ENTRY_WITH_FIELDS(detect_slope, NULL, 0, detect_slope_state_fields, sizeof(detect_slope_state_fields)/sizeof(detect_slope_state_fields[0]))
    ENTRY_WITH_FIELDS(detect_threshold, detect_threshold_config_fields, sizeof(detect_threshold_config_fields)/sizeof(detect_threshold_config_fields[0]), NULL, 0)
    ENTRY_WITH_FIELDS(detect_zero_crossing, NULL, 0, detect_zero_crossing_state_fields, sizeof(detect_zero_crossing_state_fields)/sizeof(detect_zero_crossing_state_fields[0]))
    ENTRY_WITH_FIELDS(filter_allpass, filter_allpass_config_fields, sizeof(filter_allpass_config_fields)/sizeof(filter_allpass_config_fields[0]), filter_allpass_state_fields, sizeof(filter_allpass_state_fields)/sizeof(filter_allpass_state_fields[0]))
    ENTRY_WITH_FIELDS(filter_biquad, filter_biquad_config_fields, sizeof(filter_biquad_config_fields)/sizeof(filter_biquad_config_fields[0]), filter_biquad_state_fields, sizeof(filter_biquad_state_fields)/sizeof(filter_biquad_state_fields[0]))
    ENTRY_WITH_FIELDS(filter_comb_fb, filter_comb_fb_config_fields, sizeof(filter_comb_fb_config_fields)/sizeof(filter_comb_fb_config_fields[0]), filter_comb_fb_state_fields, sizeof(filter_comb_fb_state_fields)/sizeof(filter_comb_fb_state_fields[0]))
    ENTRY_WITH_FIELDS(filter_comb_ff, filter_comb_ff_config_fields, sizeof(filter_comb_ff_config_fields)/sizeof(filter_comb_ff_config_fields[0]), filter_comb_ff_state_fields, sizeof(filter_comb_ff_state_fields)/sizeof(filter_comb_ff_state_fields[0]))
    ENTRY_WITH_FIELDS(filter_dc_block, filter_dc_block_config_fields, sizeof(filter_dc_block_config_fields)/sizeof(filter_dc_block_config_fields[0]), filter_dc_block_state_fields, sizeof(filter_dc_block_state_fields)/sizeof(filter_dc_block_state_fields[0]))
    ENTRY_WITH_FIELDS(filter_differentiate, NULL, 0, filter_differentiate_state_fields, sizeof(filter_differentiate_state_fields)/sizeof(filter_differentiate_state_fields[0]))
    ENTRY_WITH_FIELDS(filter_fir, filter_fir_config_fields, sizeof(filter_fir_config_fields)/sizeof(filter_fir_config_fields[0]), filter_fir_state_fields, sizeof(filter_fir_state_fields)/sizeof(filter_fir_state_fields[0]))
    ENTRY_WITH_FIELDS(filter_integrate, NULL, 0, filter_integrate_state_fields, sizeof(filter_integrate_state_fields)/sizeof(filter_integrate_state_fields[0]))
    ENTRY_WITH_FIELDS(freq_fft, freq_fft_config_fields, sizeof(freq_fft_config_fields)/sizeof(freq_fft_config_fields[0]), NULL, 0)
    ENTRY_WITH_FIELDS(freq_ifft, freq_ifft_config_fields, sizeof(freq_ifft_config_fields)/sizeof(freq_ifft_config_fields[0]), NULL, 0)
    ENTRY_WITH_FIELDS(freq_multiply, freq_multiply_config_fields, sizeof(freq_multiply_config_fields)/sizeof(freq_multiply_config_fields[0]), NULL, 0)
    ENTRY_WITH_FIELDS(freq_overlap_add, freq_overlap_add_config_fields, sizeof(freq_overlap_add_config_fields)/sizeof(freq_overlap_add_config_fields[0]), freq_overlap_add_state_fields, sizeof(freq_overlap_add_state_fields)/sizeof(freq_overlap_add_state_fields[0]))
    ENTRY_WITH_FIELDS(freq_overlap_save, freq_overlap_save_config_fields, sizeof(freq_overlap_save_config_fields)/sizeof(freq_overlap_save_config_fields[0]), freq_overlap_save_state_fields, sizeof(freq_overlap_save_state_fields)/sizeof(freq_overlap_save_state_fields[0]))
    ENTRY_WITH_FIELDS(freq_window, freq_window_config_fields, sizeof(freq_window_config_fields)/sizeof(freq_window_config_fields[0]), NULL, 0)
    ENTRY_WITH_FIELDS(generation_dc, generation_dc_config_fields, sizeof(generation_dc_config_fields)/sizeof(generation_dc_config_fields[0]), NULL, 0)
    ENTRY_WITH_FIELDS(generation_envelope, generation_envelope_config_fields, sizeof(generation_envelope_config_fields)/sizeof(generation_envelope_config_fields[0]), generation_envelope_state_fields, sizeof(generation_envelope_state_fields)/sizeof(generation_envelope_state_fields[0]))
    ENTRY_WITH_FIELDS(generation_impulse, generation_impulse_config_fields, sizeof(generation_impulse_config_fields)/sizeof(generation_impulse_config_fields[0]), generation_impulse_state_fields, sizeof(generation_impulse_state_fields)/sizeof(generation_impulse_state_fields[0]))
    ENTRY_WITH_FIELDS(generation_lfo, generation_lfo_config_fields, sizeof(generation_lfo_config_fields)/sizeof(generation_lfo_config_fields[0]), generation_lfo_state_fields, sizeof(generation_lfo_state_fields)/sizeof(generation_lfo_state_fields[0]))
    ENTRY_WITH_FIELDS(generation_noise, generation_noise_config_fields, sizeof(generation_noise_config_fields)/sizeof(generation_noise_config_fields[0]), generation_noise_state_fields, sizeof(generation_noise_state_fields)/sizeof(generation_noise_state_fields[0]))
    ENTRY_WITH_FIELDS(generation_oscillator, generation_oscillator_config_fields, sizeof(generation_oscillator_config_fields)/sizeof(generation_oscillator_config_fields[0]), generation_oscillator_state_fields, sizeof(generation_oscillator_state_fields)/sizeof(generation_oscillator_state_fields[0]))
    ENTRY_WITH_FIELDS(interpolation_cubic, NULL, 0, NULL, 0)
    ENTRY_WITH_FIELDS(interpolation_lagrange, interpolation_lagrange_config_fields, sizeof(interpolation_lagrange_config_fields)/sizeof(interpolation_lagrange_config_fields[0]), interpolation_lagrange_state_fields, sizeof(interpolation_lagrange_state_fields)/sizeof(interpolation_lagrange_state_fields[0]))
    ENTRY_WITH_FIELDS(interpolation_linear, NULL, 0, NULL, 0)
    ENTRY_WITH_FIELDS(interpolation_sinc, interpolation_sinc_config_fields, sizeof(interpolation_sinc_config_fields)/sizeof(interpolation_sinc_config_fields[0]), interpolation_sinc_state_fields, sizeof(interpolation_sinc_state_fields)/sizeof(interpolation_sinc_state_fields[0]))
    ENTRY_WITH_FIELDS(mix_crossfade, mix_crossfade_config_fields, sizeof(mix_crossfade_config_fields)/sizeof(mix_crossfade_config_fields[0]), NULL, 0)
    ENTRY_WITH_FIELDS(mix_decode_ms, NULL, 0, NULL, 0)
    ENTRY_WITH_FIELDS(mix_encode_ms, NULL, 0, NULL, 0)
    ENTRY_WITH_FIELDS(mix_matrix, mix_matrix_config_fields, sizeof(mix_matrix_config_fields)/sizeof(mix_matrix_config_fields[0]), NULL, 0)
    ENTRY_WITH_FIELDS(mix_pan_stereo, mix_pan_stereo_config_fields, sizeof(mix_pan_stereo_config_fields)/sizeof(mix_pan_stereo_config_fields[0]), NULL, 0)
    ENTRY_WITH_FIELDS(mix_wet_dry, mix_wet_dry_config_fields, sizeof(mix_wet_dry_config_fields)/sizeof(mix_wet_dry_config_fields[0]), NULL, 0)
    ENTRY_WITH_FIELDS(modulation_amplitude, modulation_amplitude_config_fields, sizeof(modulation_amplitude_config_fields)/sizeof(modulation_amplitude_config_fields[0]), NULL, 0)
    ENTRY_WITH_FIELDS(modulation_frequency, modulation_frequency_config_fields, sizeof(modulation_frequency_config_fields)/sizeof(modulation_frequency_config_fields[0]), modulation_frequency_state_fields, sizeof(modulation_frequency_state_fields)/sizeof(modulation_frequency_state_fields[0]))
    ENTRY_WITH_FIELDS(modulation_phase, modulation_phase_config_fields, sizeof(modulation_phase_config_fields)/sizeof(modulation_phase_config_fields[0]), modulation_phase_state_fields, sizeof(modulation_phase_state_fields)/sizeof(modulation_phase_state_fields[0]))
    ENTRY_WITH_FIELDS(modulation_ring, NULL, 0, NULL, 0)
    ENTRY_WITH_FIELDS(modulation_scrub, modulation_scrub_config_fields, sizeof(modulation_scrub_config_fields)/sizeof(modulation_scrub_config_fields[0]), NULL, 0)
    ENTRY_WITH_FIELDS(nonlinear_bitcrush, nonlinear_bitcrush_config_fields, sizeof(nonlinear_bitcrush_config_fields)/sizeof(nonlinear_bitcrush_config_fields[0]), NULL, 0)
    ENTRY_WITH_FIELDS(nonlinear_samplerate_reduce, nonlinear_samplerate_reduce_config_fields, sizeof(nonlinear_samplerate_reduce_config_fields)/sizeof(nonlinear_samplerate_reduce_config_fields[0]), nonlinear_samplerate_reduce_state_fields, sizeof(nonlinear_samplerate_reduce_state_fields)/sizeof(nonlinear_samplerate_reduce_state_fields[0]))
    ENTRY_WITH_FIELDS(nonlinear_waveshape, nonlinear_waveshape_config_fields, sizeof(nonlinear_waveshape_config_fields)/sizeof(nonlinear_waveshape_config_fields[0]), NULL, 0)
    ENTRY_WITH_FIELDS(src_antialias, src_antialias_config_fields, sizeof(src_antialias_config_fields)/sizeof(src_antialias_config_fields[0]), src_antialias_state_fields, sizeof(src_antialias_state_fields)/sizeof(src_antialias_state_fields[0]))
    ENTRY_WITH_FIELDS(src_antiimage, src_antiimage_config_fields, sizeof(src_antiimage_config_fields)/sizeof(src_antiimage_config_fields[0]), src_antiimage_state_fields, sizeof(src_antiimage_state_fields)/sizeof(src_antiimage_state_fields[0]))
    ENTRY_WITH_FIELDS(src_convert_format, src_convert_format_config_fields, sizeof(src_convert_format_config_fields)/sizeof(src_convert_format_config_fields[0]), NULL, 0)
    ENTRY_WITH_FIELDS(src_downsample, src_downsample_config_fields, sizeof(src_downsample_config_fields)/sizeof(src_downsample_config_fields[0]), NULL, 0)
    ENTRY_WITH_FIELDS(src_upsample, src_upsample_config_fields, sizeof(src_upsample_config_fields)/sizeof(src_upsample_config_fields[0]), NULL, 0)
};

static const int g_registry_count = sizeof(g_registry) / sizeof(g_registry[0]);

void atom_registry_init(void) {}

const atom_registry_entry_t *atom_registry_find(const char *name) {
    for (int i = 0; i < g_registry_count; i++) {
        if (strcmp(g_registry[i].name, name) == 0) return &g_registry[i];
    }
    return NULL;
}

int atom_registry_count(void) { return g_registry_count; }

const atom_registry_entry_t *atom_registry_get(int index) {
    if (index < 0 || index >= g_registry_count) return NULL;
    return &g_registry[index];
}
