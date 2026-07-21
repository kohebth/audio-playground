/* Generated from schema/atoms/atoms.json by tools/generate_atom_artifacts.pl. Do not edit. */

static const apg_atom_contract_field_t catalog_amplitude_accumulate_inputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_amplitude_accumulate_outputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_amplitude_latch_inputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
    {"gate", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_amplitude_latch_outputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_amplitude_latch_config[] = {
    {"threshold", APG_ATOM_FIELD_FLOAT, true, "float", "0.5", true, -1, true, 1, "ratio", NULL, true, false, 0.0, false, NULL, NULL, 0u},
};

static const apg_atom_contract_field_t catalog_amplitude_add_inputs[] = {
    {"signal_a", APG_ATOM_FIELD_SIGNAL, true},
    {"signal_b", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_amplitude_add_outputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_amplitude_clip_hard_inputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_amplitude_clip_hard_outputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_amplitude_clip_hard_config[] = {
    {"threshold", APG_ATOM_FIELD_FLOAT, true, "float", "1", true, 0.001, true, 4, "ratio", "logarithmic", true, false, 0.0, false, NULL, NULL, 0u},
};

static const apg_atom_contract_field_t catalog_amplitude_clip_soft_inputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_amplitude_clip_soft_outputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const char *const catalog_amplitude_clip_soft_config_curve_options[] = {"cubic", "tanh"};
static const int catalog_amplitude_clip_soft_config_curve_option_values[] = {0, 1};

static const apg_atom_contract_field_t catalog_amplitude_clip_soft_config[] = {
    {"threshold", APG_ATOM_FIELD_FLOAT, true, "float", "1", true, 0.001, true, 4, "ratio", "logarithmic", true, false, 0.0, false, NULL, NULL, 0u},
    {"curve", APG_ATOM_FIELD_INT, true, "enum", "0", false, 0.0, false, 0.0, NULL, NULL, true, false, 0.0, false, catalog_amplitude_clip_soft_config_curve_options, catalog_amplitude_clip_soft_config_curve_option_values, FIELD_COUNT(catalog_amplitude_clip_soft_config_curve_options)},
};

static const apg_atom_contract_field_t catalog_amplitude_divide_inputs[] = {
    {"numerator", APG_ATOM_FIELD_SIGNAL, true},
    {"denominator", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_amplitude_divide_outputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_amplitude_divide_config[] = {
    {"epsilon", APG_ATOM_FIELD_FLOAT, true, "float", "1e-06", true, 1e-12, true, 0.1, NULL, "logarithmic", true, false, 0.0, false, NULL, NULL, 0u},
};

static const apg_atom_contract_field_t catalog_amplitude_multiply_inputs[] = {
    {"signal_a", APG_ATOM_FIELD_SIGNAL, true},
    {"signal_b", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_amplitude_multiply_outputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_amplitude_normalize_inputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_amplitude_normalize_outputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const char *const catalog_amplitude_normalize_config_mode_options[] = {"peak", "rms"};
static const int catalog_amplitude_normalize_config_mode_option_values[] = {0, 1};

static const apg_atom_contract_field_t catalog_amplitude_normalize_config[] = {
    {"target_level", APG_ATOM_FIELD_FLOAT, true, "float", "1", true, 0.001, true, 1, "ratio", "logarithmic", true, false, 0.0, false, NULL, NULL, 0u},
    {"mode", APG_ATOM_FIELD_INT, true, "enum", "0", false, 0.0, false, 0.0, NULL, NULL, true, false, 0.0, false, catalog_amplitude_normalize_config_mode_options, catalog_amplitude_normalize_config_mode_option_values, FIELD_COUNT(catalog_amplitude_normalize_config_mode_options)},
};

static const apg_atom_contract_field_t catalog_amplitude_smooth_inputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_amplitude_smooth_outputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_amplitude_smooth_config[] = {
    {"attack", APG_ATOM_FIELD_FLOAT, true, "float", "0.01", true, 0, true, 60, NULL, NULL, true, false, 0.0, false, NULL, NULL, 0u},
    {"release", APG_ATOM_FIELD_FLOAT, true, "float", "0.1", true, 0, true, 60, NULL, NULL, true, false, 0.0, false, NULL, NULL, 0u},
};

static const apg_atom_contract_field_t catalog_amplitude_subtract_inputs[] = {
    {"signal_a", APG_ATOM_FIELD_SIGNAL, true},
    {"signal_b", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_amplitude_subtract_outputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_delay_fractional_inputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_delay_fractional_outputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const char *const catalog_delay_fractional_config_interpolation_options[] = {"linear", "cubic", "sinc", "lagrange"};
static const int catalog_delay_fractional_config_interpolation_option_values[] = {0, 1, 2, 3};

static const apg_atom_contract_field_t catalog_delay_fractional_config[] = {
    {"delay_samples", APG_ATOM_FIELD_FLOAT, true, "float", "1", true, 0, true, 191999, "samples", NULL, true, false, 0.0, false, NULL, NULL, 0u},
    {"interpolation", APG_ATOM_FIELD_INT, true, "enum", "0", false, 0.0, false, 0.0, NULL, NULL, true, false, 0.0, false, catalog_delay_fractional_config_interpolation_options, catalog_delay_fractional_config_interpolation_option_values, FIELD_COUNT(catalog_delay_fractional_config_interpolation_options)},
};

static const apg_atom_contract_field_t catalog_delay_line_inputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_delay_line_outputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_delay_line_config[] = {
    {"length", APG_ATOM_FIELD_INT, true, "int", "1", true, 1, true, 192000, "samples", NULL, false, false, 0.0, true, NULL, NULL, 0u},
};

static const apg_atom_contract_field_t catalog_delay_tap_feedback_inputs[] = {
    {"buffer", APG_ATOM_FIELD_BUFFER, true},
    {"tap_position", APG_ATOM_FIELD_SCALAR, true},
};

static const apg_atom_contract_field_t catalog_delay_tap_feedback_outputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_delay_tap_feedback_config[] = {
    {"coefficient", APG_ATOM_FIELD_FLOAT, true, "float", "0.5", true, -1, true, 1, "ratio", NULL, true, false, 0.0, false, NULL, NULL, 0u},
};

static const apg_atom_contract_field_t catalog_delay_tap_feedforward_inputs[] = {
    {"buffer", APG_ATOM_FIELD_BUFFER, true},
    {"tap_position", APG_ATOM_FIELD_SCALAR, true},
};

static const apg_atom_contract_field_t catalog_delay_tap_feedforward_outputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_delay_tap_feedforward_config[] = {
    {"coefficient", APG_ATOM_FIELD_FLOAT, true, "float", "0.5", true, -1, true, 1, "ratio", NULL, true, false, 0.0, false, NULL, NULL, 0u},
};

static const apg_atom_contract_field_t catalog_delay_unit_inputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_delay_unit_outputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_detect_autocorrelate_inputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_detect_autocorrelate_outputs[] = {
    {"correlation", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_detect_autocorrelate_config[] = {
    {"max_lag", APG_ATOM_FIELD_INT, true, "int", "512", true, 1, true, 1024, "samples", NULL, false, false, 0.0, true, NULL, NULL, 0u},
};

static const apg_atom_contract_field_t catalog_detect_pitch_inputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_detect_pitch_outputs[] = {
    {"pitch", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_detect_pitch_config[] = {
    {"max_lag", APG_ATOM_FIELD_INT, true, "int", "512", true, 33, true, 1024, "samples", NULL, false, false, 0.0, true, NULL, NULL, 0u},
};

static const apg_atom_contract_field_t catalog_detect_envelope_inputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_detect_envelope_outputs[] = {
    {"envelope", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_detect_envelope_config[] = {
    {"attack", APG_ATOM_FIELD_FLOAT, true, "float", "0.01", true, 0, true, 60, NULL, NULL, true, false, 0.0, false, NULL, NULL, 0u},
    {"release", APG_ATOM_FIELD_FLOAT, true, "float", "0.1", true, 0, true, 60, NULL, NULL, true, false, 0.0, false, NULL, NULL, 0u},
};

static const apg_atom_contract_field_t catalog_detect_peak_inputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_detect_peak_outputs[] = {
    {"level", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_detect_peak_config[] = {
    {"attack", APG_ATOM_FIELD_FLOAT, true, "float", "0.01", true, 0, true, 60, NULL, NULL, true, false, 0.0, false, NULL, NULL, 0u},
    {"release", APG_ATOM_FIELD_FLOAT, true, "float", "0.1", true, 0, true, 60, NULL, NULL, true, false, 0.0, false, NULL, NULL, 0u},
};

static const apg_atom_contract_field_t catalog_detect_rms_inputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_detect_rms_outputs[] = {
    {"level", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_detect_rms_config[] = {
    {"window_size", APG_ATOM_FIELD_INT, true, "int", "256", true, 1, true, 4096, "samples", NULL, false, false, 0.0, true, NULL, NULL, 0u},
};

static const apg_atom_contract_field_t catalog_detect_slope_inputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_detect_slope_outputs[] = {
    {"slope", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_detect_threshold_inputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_detect_threshold_outputs[] = {
    {"gate", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_detect_threshold_config[] = {
    {"threshold", APG_ATOM_FIELD_FLOAT, true, "float", "0.5", true, -1, true, 1, "ratio", NULL, true, false, 0.0, false, NULL, NULL, 0u},
};

static const apg_atom_contract_field_t catalog_detect_zero_crossing_inputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_detect_zero_crossing_outputs[] = {
    {"trigger", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_filter_allpass_inputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_filter_allpass_outputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_filter_allpass_config[] = {
    {"delay_samples", APG_ATOM_FIELD_INT, true, "int", "1", true, 1, true, 47999, "samples", NULL, false, false, 0.0, true, NULL, NULL, 0u},
    {"coefficient", APG_ATOM_FIELD_FLOAT, true, "float", "0.5", true, -0.999, true, 0.999, "ratio", NULL, true, false, 0.0, false, NULL, NULL, 0u},
};

static const apg_atom_contract_field_t catalog_filter_biquad_coefficients_inputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_filter_biquad_coefficients_outputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_filter_biquad_coefficients_config[] = {
    {"b0", APG_ATOM_FIELD_FLOAT, true, "float", "1", true, -4, true, 4, "ratio", NULL, true, false, 0.0, false, NULL, NULL, 0u},
    {"b1", APG_ATOM_FIELD_FLOAT, true, "float", "0", true, -4, true, 4, "ratio", NULL, true, false, 0.0, false, NULL, NULL, 0u},
    {"b2", APG_ATOM_FIELD_FLOAT, true, "float", "0", true, -4, true, 4, "ratio", NULL, true, false, 0.0, false, NULL, NULL, 0u},
    {"a1", APG_ATOM_FIELD_FLOAT, true, "float", "0", true, -2, true, 2, "ratio", NULL, true, false, 0.0, false, NULL, NULL, 0u},
    {"a2", APG_ATOM_FIELD_FLOAT, true, "float", "0", true, -2, true, 2, "ratio", NULL, true, false, 0.0, false, NULL, NULL, 0u},
};

static const apg_atom_contract_field_t catalog_filter_biquad_inputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
    {"cutoff", APG_ATOM_FIELD_SIGNAL_OPTIONAL, false},
};

static const apg_atom_contract_field_t catalog_filter_biquad_outputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const char *const catalog_filter_biquad_config_mode_options[] = {"lowpass", "highpass", "bandpass", "notch"};
static const int catalog_filter_biquad_config_mode_option_values[] = {0, 1, 2, 3};

static const apg_atom_contract_field_t catalog_filter_biquad_config[] = {
    {"cutoff", APG_ATOM_FIELD_FLOAT, true, "float", "1000", true, 20, true, 20000, "hz", "logarithmic", true, true, 10, false, NULL, NULL, 0u},
    {"q", APG_ATOM_FIELD_FLOAT, true, "float", "0.707", true, 0.05, true, 20, "ratio", "logarithmic", true, true, 10, false, NULL, NULL, 0u},
    {"mode", APG_ATOM_FIELD_INT, true, "enum", "0", false, 0.0, false, 0.0, NULL, NULL, true, false, 0.0, false, catalog_filter_biquad_config_mode_options, catalog_filter_biquad_config_mode_option_values, FIELD_COUNT(catalog_filter_biquad_config_mode_options)},
    {"smoothing_ms", APG_ATOM_FIELD_FLOAT, true, "float", "10", true, 0, true, 1000, "ms", NULL, true, false, 0.0, false, NULL, NULL, 0u},
};

static const apg_atom_contract_field_t catalog_filter_comb_fb_inputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
    {"delay", APG_ATOM_FIELD_SIGNAL_OPTIONAL, false},
};

static const apg_atom_contract_field_t catalog_filter_comb_fb_outputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_filter_comb_fb_config[] = {
    {"delay_samples", APG_ATOM_FIELD_INT, true, "int", "1", true, 1, true, 47999, "samples", NULL, false, false, 0.0, true, NULL, NULL, 0u},
    {"coefficient", APG_ATOM_FIELD_FLOAT, true, "float", "0.5", true, -0.999, true, 0.999, "ratio", NULL, true, false, 0.0, false, NULL, NULL, 0u},
};

static const apg_atom_contract_field_t catalog_filter_comb_ff_inputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_filter_comb_ff_outputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_filter_comb_ff_config[] = {
    {"delay_samples", APG_ATOM_FIELD_INT, true, "int", "1", true, 1, true, 47999, "samples", NULL, false, false, 0.0, true, NULL, NULL, 0u},
    {"coefficient", APG_ATOM_FIELD_FLOAT, true, "float", "0.5", true, -4, true, 4, "ratio", NULL, true, false, 0.0, false, NULL, NULL, 0u},
};

static const apg_atom_contract_field_t catalog_filter_dc_block_inputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_filter_dc_block_outputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_filter_dc_block_config[] = {
    {"coefficient", APG_ATOM_FIELD_FLOAT, true, "float", "0.995", true, 0.9, true, 0.99999, "ratio", NULL, true, false, 0.0, false, NULL, NULL, 0u},
};

static const apg_atom_contract_field_t catalog_filter_differentiate_inputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_filter_differentiate_outputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_filter_fir_inputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_filter_fir_outputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_filter_fir_config[] = {
    {"kernel", APG_ATOM_FIELD_BUFFER, true, "buffer", "\"\"", false, 0.0, false, 0.0, NULL, NULL, false, false, 0.0, true, NULL, NULL, 0u},
    {"kernel_size", APG_ATOM_FIELD_INT, true, "int", "0", true, 0, true, 1024, "samples", NULL, false, false, 0.0, true, NULL, NULL, 0u},
};

static const apg_atom_contract_field_t catalog_filter_integrate_inputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_filter_integrate_outputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_freq_fft_inputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_freq_fft_outputs[] = {
    {"real", APG_ATOM_FIELD_SIGNAL, true},
    {"imag", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_freq_fft_config[] = {
    {"block_size", APG_ATOM_FIELD_INT, true, "int", "512", true, 2, true, 16384, "samples", NULL, false, false, 0.0, true, NULL, NULL, 0u},
};

static const apg_atom_contract_field_t catalog_freq_ifft_inputs[] = {
    {"real", APG_ATOM_FIELD_SIGNAL, true},
    {"imag", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_freq_ifft_outputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_freq_ifft_config[] = {
    {"block_size", APG_ATOM_FIELD_INT, true, "int", "512", true, 2, true, 16384, "samples", NULL, false, false, 0.0, true, NULL, NULL, 0u},
};

static const apg_atom_contract_field_t catalog_freq_multiply_inputs[] = {
    {"real_a", APG_ATOM_FIELD_SIGNAL, true},
    {"imag_a", APG_ATOM_FIELD_SIGNAL, true},
    {"real_b", APG_ATOM_FIELD_SIGNAL, true},
    {"imag_b", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_freq_multiply_outputs[] = {
    {"real", APG_ATOM_FIELD_SIGNAL, true},
    {"imag", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_freq_multiply_config[] = {
    {"block_size", APG_ATOM_FIELD_INT, true, "int", "512", true, 2, true, 16384, "samples", NULL, false, false, 0.0, true, NULL, NULL, 0u},
};

static const apg_atom_contract_field_t catalog_freq_overlap_add_inputs[] = {
    {"frame", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_freq_overlap_add_outputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_freq_overlap_add_config[] = {
    {"block_size", APG_ATOM_FIELD_INT, true, "int", "512", true, 2, true, 16384, "samples", NULL, false, false, 0.0, true, NULL, NULL, 0u},
    {"hop_size", APG_ATOM_FIELD_INT, true, "int", "256", true, 1, true, 16384, "samples", NULL, false, false, 0.0, true, NULL, NULL, 0u},
};

static const apg_atom_contract_field_t catalog_freq_overlap_save_inputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_freq_overlap_save_outputs[] = {
    {"frame", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_freq_overlap_save_config[] = {
    {"block_size", APG_ATOM_FIELD_INT, true, "int", "512", true, 2, true, 16384, "samples", NULL, false, false, 0.0, true, NULL, NULL, 0u},
    {"hop_size", APG_ATOM_FIELD_INT, true, "int", "256", true, 1, true, 16384, "samples", NULL, false, false, 0.0, true, NULL, NULL, 0u},
};

static const apg_atom_contract_field_t catalog_freq_window_inputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_freq_window_outputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const char *const catalog_freq_window_config_window_type_options[] = {"hann", "hamming", "blackman", "rectangular"};
static const int catalog_freq_window_config_window_type_option_values[] = {0, 1, 2, 3};

static const apg_atom_contract_field_t catalog_freq_window_config[] = {
    {"window_type", APG_ATOM_FIELD_INT, true, "enum", "0", false, 0.0, false, 0.0, NULL, NULL, true, false, 0.0, false, catalog_freq_window_config_window_type_options, catalog_freq_window_config_window_type_option_values, FIELD_COUNT(catalog_freq_window_config_window_type_options)},
    {"block_size", APG_ATOM_FIELD_INT, true, "int", "512", true, 2, true, 16384, "samples", NULL, false, false, 0.0, true, NULL, NULL, 0u},
};

static const apg_atom_contract_field_t catalog_freq_shift_inputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
    {"pitch_shift", APG_ATOM_FIELD_SIGNAL_OPTIONAL, false},
};

static const apg_atom_contract_field_t catalog_freq_shift_outputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_freq_shift_config[] = {
    {"block_size", APG_ATOM_FIELD_INT, true, "int", "1024", true, 2, true, 16384, "samples", NULL, false, false, 0.0, true, NULL, NULL, 0u},
};

static const apg_atom_contract_field_t catalog_generation_dc_outputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_generation_dc_config[] = {
    {"value", APG_ATOM_FIELD_SCALAR, true, "float", "0", true, -4, true, 4, "ratio", NULL, true, false, 0.0, false, NULL, NULL, 0u},
};

static const apg_atom_contract_field_t catalog_generation_envelope_inputs[] = {
    {"gate", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_generation_envelope_outputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_generation_envelope_config[] = {
    {"attack", APG_ATOM_FIELD_FLOAT, true, "float", "0.01", true, 0, true, 60, NULL, NULL, true, false, 0.0, false, NULL, NULL, 0u},
    {"decay", APG_ATOM_FIELD_FLOAT, true, "float", "0.1", true, 0, true, 60, NULL, NULL, true, false, 0.0, false, NULL, NULL, 0u},
    {"sustain", APG_ATOM_FIELD_FLOAT, true, "float", "0.7", true, 0, true, 1, "ratio", NULL, true, false, 0.0, false, NULL, NULL, 0u},
    {"release", APG_ATOM_FIELD_FLOAT, true, "float", "0.2", true, 0, true, 60, NULL, NULL, true, false, 0.0, false, NULL, NULL, 0u},
};

static const apg_atom_contract_field_t catalog_generation_impulse_outputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_generation_impulse_config[] = {
    {"interval", APG_ATOM_FIELD_FLOAT, true, "float", "1", true, 0, true, 3600, NULL, NULL, true, false, 0.0, false, NULL, NULL, 0u},
};

static const apg_atom_contract_field_t catalog_generation_lfo_outputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const char *const catalog_generation_lfo_config_waveform_options[] = {"sine", "saw", "square", "triangle"};
static const int catalog_generation_lfo_config_waveform_option_values[] = {0, 1, 2, 3};

static const apg_atom_contract_field_t catalog_generation_lfo_config[] = {
    {"frequency", APG_ATOM_FIELD_FLOAT, true, "float", "1", true, 0.001, true, 20, "hz", "logarithmic", true, false, 0.0, false, NULL, NULL, 0u},
    {"waveform", APG_ATOM_FIELD_INT, true, "enum", "0", false, 0.0, false, 0.0, NULL, NULL, true, false, 0.0, false, catalog_generation_lfo_config_waveform_options, catalog_generation_lfo_config_waveform_option_values, FIELD_COUNT(catalog_generation_lfo_config_waveform_options)},
    {"phase_offset", APG_ATOM_FIELD_FLOAT, true, "float", "0", true, 0, true, 1, "ratio", NULL, true, false, 0.0, false, NULL, NULL, 0u},
};

static const apg_atom_contract_field_t catalog_generation_noise_outputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const char *const catalog_generation_noise_config_color_options[] = {"white", "pink", "brown"};
static const int catalog_generation_noise_config_color_option_values[] = {4, 5, 6};

static const apg_atom_contract_field_t catalog_generation_noise_config[] = {
    {"amplitude", APG_ATOM_FIELD_FLOAT, true, "float", "0.2", true, 0, true, 1, "ratio", NULL, true, false, 0.0, false, NULL, NULL, 0u},
    {"color", APG_ATOM_FIELD_INT, true, "enum", "4", false, 0.0, false, 0.0, NULL, NULL, true, false, 0.0, false, catalog_generation_noise_config_color_options, catalog_generation_noise_config_color_option_values, FIELD_COUNT(catalog_generation_noise_config_color_options)},
};

static const apg_atom_contract_field_t catalog_generation_oscillator_inputs[] = {
    {"frequency", APG_ATOM_FIELD_SIGNAL_OPTIONAL, false},
};

static const apg_atom_contract_field_t catalog_generation_oscillator_outputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const char *const catalog_generation_oscillator_config_waveform_options[] = {"sine", "saw", "square", "triangle"};
static const int catalog_generation_oscillator_config_waveform_option_values[] = {0, 1, 2, 3};

static const apg_atom_contract_field_t catalog_generation_oscillator_config[] = {
    {"frequency", APG_ATOM_FIELD_FLOAT, true, "float", "440", true, 0.01, true, 20000, "hz", "logarithmic", true, false, 0.0, false, NULL, NULL, 0u},
    {"waveform", APG_ATOM_FIELD_INT, true, "enum", "0", false, 0.0, false, 0.0, NULL, NULL, true, false, 0.0, false, catalog_generation_oscillator_config_waveform_options, catalog_generation_oscillator_config_waveform_option_values, FIELD_COUNT(catalog_generation_oscillator_config_waveform_options)},
    {"phase_offset", APG_ATOM_FIELD_FLOAT, true, "float", "0", true, 0, true, 1, "ratio", NULL, true, false, 0.0, false, NULL, NULL, 0u},
};

static const apg_atom_contract_field_t catalog_interpolation_cubic_inputs[] = {
    {"signal_n1", APG_ATOM_FIELD_SIGNAL, true},
    {"signal_a", APG_ATOM_FIELD_SIGNAL, true},
    {"signal_b", APG_ATOM_FIELD_SIGNAL, true},
    {"signal_c", APG_ATOM_FIELD_SIGNAL, true},
    {"t", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_interpolation_cubic_outputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_interpolation_lagrange_inputs[] = {
    {"samples", APG_ATOM_FIELD_SIGNAL, true},
    {"t", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_interpolation_lagrange_outputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_interpolation_lagrange_config[] = {
    {"order", APG_ATOM_FIELD_INT, true, "int", "3", true, 1, true, 16, NULL, NULL, false, false, 0.0, true, NULL, NULL, 0u},
};

static const apg_atom_contract_field_t catalog_interpolation_linear_inputs[] = {
    {"signal_a", APG_ATOM_FIELD_SIGNAL, true},
    {"signal_b", APG_ATOM_FIELD_SIGNAL, true},
    {"t", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_interpolation_linear_outputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_interpolation_sinc_inputs[] = {
    {"buffer", APG_ATOM_FIELD_BUFFER, true},
    {"position", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_interpolation_sinc_outputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_interpolation_sinc_config[] = {
    {"num_taps", APG_ATOM_FIELD_INT, true, "int", "32", true, 2, true, 128, "samples", NULL, false, false, 0.0, true, NULL, NULL, 0u},
};

static const apg_atom_contract_field_t catalog_mix_crossfade_inputs[] = {
    {"signal_a", APG_ATOM_FIELD_SIGNAL, true},
    {"signal_b", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_mix_crossfade_outputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const char *const catalog_mix_crossfade_config_curve_options[] = {"linear", "equal_power"};
static const int catalog_mix_crossfade_config_curve_option_values[] = {0, 1};

static const apg_atom_contract_field_t catalog_mix_crossfade_config[] = {
    {"t", APG_ATOM_FIELD_FLOAT, true, "float", "0.5", true, 0, true, 1, "ratio", NULL, true, false, 0.0, false, NULL, NULL, 0u},
    {"curve", APG_ATOM_FIELD_INT, false, "enum", "0", false, 0.0, false, 0.0, NULL, NULL, true, false, 0.0, false, catalog_mix_crossfade_config_curve_options, catalog_mix_crossfade_config_curve_option_values, FIELD_COUNT(catalog_mix_crossfade_config_curve_options)},
};

static const apg_atom_contract_field_t catalog_mix_decode_ms_inputs[] = {
    {"mid", APG_ATOM_FIELD_SIGNAL, true},
    {"side", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_mix_decode_ms_outputs[] = {
    {"left", APG_ATOM_FIELD_SIGNAL, true},
    {"right", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_mix_encode_ms_inputs[] = {
    {"left", APG_ATOM_FIELD_SIGNAL, true},
    {"right", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_mix_encode_ms_outputs[] = {
    {"mid", APG_ATOM_FIELD_SIGNAL, true},
    {"side", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_mix_matrix_inputs[] = {
    {"signals", APG_ATOM_FIELD_SIGNAL_ARRAY, true},
};

static const apg_atom_contract_field_t catalog_mix_matrix_outputs[] = {
    {"signals", APG_ATOM_FIELD_SIGNAL_ARRAY, true},
};

static const apg_atom_contract_field_t catalog_mix_matrix_config[] = {
    {"coefficients", APG_ATOM_FIELD_FLOAT_MATRIX, true, "float_matrix", "[[1]]", false, 0.0, false, 0.0, NULL, NULL, false, false, 0.0, true, NULL, NULL, 0u},
};

static const apg_atom_contract_field_t catalog_mix_pan_stereo_inputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_mix_pan_stereo_outputs[] = {
    {"left", APG_ATOM_FIELD_SIGNAL, true},
    {"right", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_mix_pan_stereo_config[] = {
    {"position", APG_ATOM_FIELD_FLOAT, true, "float", "0", true, -1, true, 1, "ratio", NULL, true, false, 0.0, false, NULL, NULL, 0u},
};

static const apg_atom_contract_field_t catalog_mix_wet_dry_inputs[] = {
    {"dry", APG_ATOM_FIELD_SIGNAL, true},
    {"wet", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_mix_wet_dry_outputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_mix_wet_dry_config[] = {
    {"mix", APG_ATOM_FIELD_FLOAT, true, "float", "0.5", true, 0, true, 1, "ratio", NULL, true, false, 0.0, false, NULL, NULL, 0u},
};

static const apg_atom_contract_field_t catalog_modulation_amplitude_inputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
    {"modulator", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_modulation_amplitude_outputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_modulation_amplitude_config[] = {
    {"depth", APG_ATOM_FIELD_FLOAT, true, "float", "0.5", true, 0, true, 2, "ratio", NULL, true, false, 0.0, false, NULL, NULL, 0u},
};

static const apg_atom_contract_field_t catalog_modulation_frequency_inputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
    {"modulator", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_modulation_frequency_outputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_modulation_frequency_config[] = {
    {"depth", APG_ATOM_FIELD_FLOAT, true, "float", "1", true, -4094, true, 4094, "samples", NULL, true, false, 0.0, false, NULL, NULL, 0u},
};

static const apg_atom_contract_field_t catalog_modulation_phase_inputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
    {"modulator", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_modulation_phase_outputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_modulation_phase_config[] = {
    {"depth", APG_ATOM_FIELD_FLOAT, true, "float", "1", true, 0, true, 4094, "samples", NULL, true, false, 0.0, false, NULL, NULL, 0u},
};

static const apg_atom_contract_field_t catalog_modulation_phaser_inputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
    {"modulator", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_modulation_phaser_outputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_modulation_phaser_config[] = {
    {"center_frequency", APG_ATOM_FIELD_FLOAT, true, "float", "900", true, 100, true, 4000, "hz", "logarithmic", true, true, 10, false, NULL, NULL, 0u},
    {"depth", APG_ATOM_FIELD_FLOAT, true, "float", "0.7", true, 0, true, 1, "ratio", NULL, true, true, 10, false, NULL, NULL, 0u},
    {"feedback", APG_ATOM_FIELD_FLOAT, true, "float", "0.2", true, -0.85, true, 0.85, "ratio", NULL, true, true, 10, false, NULL, NULL, 0u},
};

static const apg_atom_contract_field_t catalog_modulation_ring_inputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
    {"modulator", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_modulation_ring_outputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_modulation_scrub_inputs[] = {
    {"buffer", APG_ATOM_FIELD_BUFFER, true},
    {"position", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_modulation_scrub_outputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_modulation_scrub_config[] = {
    {"buffer_size", APG_ATOM_FIELD_INT, true, "int", "1024", true, 1, true, 192000, "samples", NULL, false, false, 0.0, true, NULL, NULL, 0u},
};

static const apg_atom_contract_field_t catalog_nonlinear_bitcrush_inputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_nonlinear_bitcrush_outputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_nonlinear_bitcrush_config[] = {
    {"bit_depth", APG_ATOM_FIELD_FLOAT, true, "float", "8", true, 1, true, 24, NULL, NULL, true, false, 0.0, false, NULL, NULL, 0u},
};

static const apg_atom_contract_field_t catalog_nonlinear_sample_hold_inputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_nonlinear_sample_hold_outputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_nonlinear_sample_hold_config[] = {
    {"factor", APG_ATOM_FIELD_FLOAT, true, "float", "4", true, 1, true, 1024, "samples", NULL, true, false, 0.0, false, NULL, NULL, 0u},
};

static const apg_atom_contract_field_t catalog_nonlinear_waveshape_inputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_nonlinear_waveshape_outputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_nonlinear_waveshape_config[] = {
    {"transfer_table", APG_ATOM_FIELD_BUFFER, true, "buffer", "\"\"", false, 0.0, false, 0.0, NULL, NULL, false, false, 0.0, true, NULL, NULL, 0u},
    {"table_size", APG_ATOM_FIELD_INT, true, "int", "0", true, 0, true, 65536, "samples", NULL, false, false, 0.0, true, NULL, NULL, 0u},
};

static const apg_atom_contract_field_t catalog_src_antialias_inputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_src_antialias_outputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_src_antialias_config[] = {
    {"cutoff", APG_ATOM_FIELD_FLOAT, true, "float", "20000", true, 20, true, 20000, "hz", "logarithmic", true, false, 0.0, false, NULL, NULL, 0u},
};

static const apg_atom_contract_field_t catalog_src_antiimage_inputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_src_antiimage_outputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_src_antiimage_config[] = {
    {"cutoff", APG_ATOM_FIELD_FLOAT, true, "float", "20000", true, 20, true, 20000, "hz", "logarithmic", true, false, 0.0, false, NULL, NULL, 0u},
};

static const apg_atom_contract_field_t catalog_src_convert_format_inputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_src_convert_format_outputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const char *const catalog_src_convert_format_config_from_format_options[] = {"float32"};
static const int catalog_src_convert_format_config_from_format_option_values[] = {0};
static const char *const catalog_src_convert_format_config_to_format_options[] = {"float32"};
static const int catalog_src_convert_format_config_to_format_option_values[] = {0};

static const apg_atom_contract_field_t catalog_src_convert_format_config[] = {
    {"from_format", APG_ATOM_FIELD_INT, true, "enum", "0", false, 0.0, false, 0.0, NULL, NULL, false, false, 0.0, true, catalog_src_convert_format_config_from_format_options, catalog_src_convert_format_config_from_format_option_values, FIELD_COUNT(catalog_src_convert_format_config_from_format_options)},
    {"to_format", APG_ATOM_FIELD_INT, true, "enum", "0", false, 0.0, false, 0.0, NULL, NULL, false, false, 0.0, true, catalog_src_convert_format_config_to_format_options, catalog_src_convert_format_config_to_format_option_values, FIELD_COUNT(catalog_src_convert_format_config_to_format_options)},
};

static const apg_atom_contract_field_t catalog_src_downsample_inputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_src_downsample_outputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_src_downsample_config[] = {
    {"factor", APG_ATOM_FIELD_INT, true, "int", "2", true, 2, true, 64, "ratio", NULL, false, false, 0.0, true, NULL, NULL, 0u},
};

static const apg_atom_contract_field_t catalog_src_upsample_inputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_src_upsample_outputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_src_upsample_config[] = {
    {"factor", APG_ATOM_FIELD_INT, true, "int", "2", true, 2, true, 64, "ratio", NULL, false, false, 0.0, true, NULL, NULL, 0u},
};

static const apg_atom_contract_field_t catalog_freq_quantize_inputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_freq_quantize_outputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_math_difference_inputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_math_difference_outputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_math_integrate_inputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_math_integrate_outputs[] = {
    {"signal", APG_ATOM_FIELD_SIGNAL, true},
};

static const apg_atom_contract_field_t catalog_math_integrate_config[] = {
    {"leakage", APG_ATOM_FIELD_FLOAT, true, "float", "1", true, 0, true, 1, "ratio", NULL, true, false, 0.0, false, NULL, NULL, 0u},
};

static const apg_catalog_contract_t catalog_contracts[] = {
    {"amplitude_accumulate", APG_ATOM_VISIBILITY_INTERNAL, catalog_amplitude_accumulate_inputs, FIELD_COUNT(catalog_amplitude_accumulate_inputs), catalog_amplitude_accumulate_outputs, FIELD_COUNT(catalog_amplitude_accumulate_outputs), NULL, 0u},
    {"amplitude_latch", APG_ATOM_VISIBILITY_ADVANCED, catalog_amplitude_latch_inputs, FIELD_COUNT(catalog_amplitude_latch_inputs), catalog_amplitude_latch_outputs, FIELD_COUNT(catalog_amplitude_latch_outputs), catalog_amplitude_latch_config, FIELD_COUNT(catalog_amplitude_latch_config)},
    {"amplitude_add", APG_ATOM_VISIBILITY_ADVANCED, catalog_amplitude_add_inputs, FIELD_COUNT(catalog_amplitude_add_inputs), catalog_amplitude_add_outputs, FIELD_COUNT(catalog_amplitude_add_outputs), NULL, 0u},
    {"amplitude_clip_hard", APG_ATOM_VISIBILITY_PUBLIC, catalog_amplitude_clip_hard_inputs, FIELD_COUNT(catalog_amplitude_clip_hard_inputs), catalog_amplitude_clip_hard_outputs, FIELD_COUNT(catalog_amplitude_clip_hard_outputs), catalog_amplitude_clip_hard_config, FIELD_COUNT(catalog_amplitude_clip_hard_config)},
    {"amplitude_clip_soft", APG_ATOM_VISIBILITY_PUBLIC, catalog_amplitude_clip_soft_inputs, FIELD_COUNT(catalog_amplitude_clip_soft_inputs), catalog_amplitude_clip_soft_outputs, FIELD_COUNT(catalog_amplitude_clip_soft_outputs), catalog_amplitude_clip_soft_config, FIELD_COUNT(catalog_amplitude_clip_soft_config)},
    {"amplitude_divide", APG_ATOM_VISIBILITY_ADVANCED, catalog_amplitude_divide_inputs, FIELD_COUNT(catalog_amplitude_divide_inputs), catalog_amplitude_divide_outputs, FIELD_COUNT(catalog_amplitude_divide_outputs), catalog_amplitude_divide_config, FIELD_COUNT(catalog_amplitude_divide_config)},
    {"amplitude_multiply", APG_ATOM_VISIBILITY_PUBLIC, catalog_amplitude_multiply_inputs, FIELD_COUNT(catalog_amplitude_multiply_inputs), catalog_amplitude_multiply_outputs, FIELD_COUNT(catalog_amplitude_multiply_outputs), NULL, 0u},
    {"amplitude_normalize", APG_ATOM_VISIBILITY_PUBLIC, catalog_amplitude_normalize_inputs, FIELD_COUNT(catalog_amplitude_normalize_inputs), catalog_amplitude_normalize_outputs, FIELD_COUNT(catalog_amplitude_normalize_outputs), catalog_amplitude_normalize_config, FIELD_COUNT(catalog_amplitude_normalize_config)},
    {"amplitude_smooth", APG_ATOM_VISIBILITY_PUBLIC, catalog_amplitude_smooth_inputs, FIELD_COUNT(catalog_amplitude_smooth_inputs), catalog_amplitude_smooth_outputs, FIELD_COUNT(catalog_amplitude_smooth_outputs), catalog_amplitude_smooth_config, FIELD_COUNT(catalog_amplitude_smooth_config)},
    {"amplitude_subtract", APG_ATOM_VISIBILITY_ADVANCED, catalog_amplitude_subtract_inputs, FIELD_COUNT(catalog_amplitude_subtract_inputs), catalog_amplitude_subtract_outputs, FIELD_COUNT(catalog_amplitude_subtract_outputs), NULL, 0u},
    {"delay_fractional", APG_ATOM_VISIBILITY_PUBLIC, catalog_delay_fractional_inputs, FIELD_COUNT(catalog_delay_fractional_inputs), catalog_delay_fractional_outputs, FIELD_COUNT(catalog_delay_fractional_outputs), catalog_delay_fractional_config, FIELD_COUNT(catalog_delay_fractional_config)},
    {"delay_line", APG_ATOM_VISIBILITY_PUBLIC, catalog_delay_line_inputs, FIELD_COUNT(catalog_delay_line_inputs), catalog_delay_line_outputs, FIELD_COUNT(catalog_delay_line_outputs), catalog_delay_line_config, FIELD_COUNT(catalog_delay_line_config)},
    {"delay_tap_feedback", APG_ATOM_VISIBILITY_INTERNAL, catalog_delay_tap_feedback_inputs, FIELD_COUNT(catalog_delay_tap_feedback_inputs), catalog_delay_tap_feedback_outputs, FIELD_COUNT(catalog_delay_tap_feedback_outputs), catalog_delay_tap_feedback_config, FIELD_COUNT(catalog_delay_tap_feedback_config)},
    {"delay_tap_feedforward", APG_ATOM_VISIBILITY_INTERNAL, catalog_delay_tap_feedforward_inputs, FIELD_COUNT(catalog_delay_tap_feedforward_inputs), catalog_delay_tap_feedforward_outputs, FIELD_COUNT(catalog_delay_tap_feedforward_outputs), catalog_delay_tap_feedforward_config, FIELD_COUNT(catalog_delay_tap_feedforward_config)},
    {"delay_unit", APG_ATOM_VISIBILITY_ADVANCED, catalog_delay_unit_inputs, FIELD_COUNT(catalog_delay_unit_inputs), catalog_delay_unit_outputs, FIELD_COUNT(catalog_delay_unit_outputs), NULL, 0u},
    {"detect_autocorrelate", APG_ATOM_VISIBILITY_ADVANCED, catalog_detect_autocorrelate_inputs, FIELD_COUNT(catalog_detect_autocorrelate_inputs), catalog_detect_autocorrelate_outputs, FIELD_COUNT(catalog_detect_autocorrelate_outputs), catalog_detect_autocorrelate_config, FIELD_COUNT(catalog_detect_autocorrelate_config)},
    {"detect_pitch", APG_ATOM_VISIBILITY_PUBLIC, catalog_detect_pitch_inputs, FIELD_COUNT(catalog_detect_pitch_inputs), catalog_detect_pitch_outputs, FIELD_COUNT(catalog_detect_pitch_outputs), catalog_detect_pitch_config, FIELD_COUNT(catalog_detect_pitch_config)},
    {"detect_envelope", APG_ATOM_VISIBILITY_PUBLIC, catalog_detect_envelope_inputs, FIELD_COUNT(catalog_detect_envelope_inputs), catalog_detect_envelope_outputs, FIELD_COUNT(catalog_detect_envelope_outputs), catalog_detect_envelope_config, FIELD_COUNT(catalog_detect_envelope_config)},
    {"detect_peak", APG_ATOM_VISIBILITY_PUBLIC, catalog_detect_peak_inputs, FIELD_COUNT(catalog_detect_peak_inputs), catalog_detect_peak_outputs, FIELD_COUNT(catalog_detect_peak_outputs), catalog_detect_peak_config, FIELD_COUNT(catalog_detect_peak_config)},
    {"detect_rms", APG_ATOM_VISIBILITY_PUBLIC, catalog_detect_rms_inputs, FIELD_COUNT(catalog_detect_rms_inputs), catalog_detect_rms_outputs, FIELD_COUNT(catalog_detect_rms_outputs), catalog_detect_rms_config, FIELD_COUNT(catalog_detect_rms_config)},
    {"detect_slope", APG_ATOM_VISIBILITY_INTERNAL, catalog_detect_slope_inputs, FIELD_COUNT(catalog_detect_slope_inputs), catalog_detect_slope_outputs, FIELD_COUNT(catalog_detect_slope_outputs), NULL, 0u},
    {"detect_threshold", APG_ATOM_VISIBILITY_PUBLIC, catalog_detect_threshold_inputs, FIELD_COUNT(catalog_detect_threshold_inputs), catalog_detect_threshold_outputs, FIELD_COUNT(catalog_detect_threshold_outputs), catalog_detect_threshold_config, FIELD_COUNT(catalog_detect_threshold_config)},
    {"detect_zero_crossing", APG_ATOM_VISIBILITY_ADVANCED, catalog_detect_zero_crossing_inputs, FIELD_COUNT(catalog_detect_zero_crossing_inputs), catalog_detect_zero_crossing_outputs, FIELD_COUNT(catalog_detect_zero_crossing_outputs), NULL, 0u},
    {"filter_allpass", APG_ATOM_VISIBILITY_ADVANCED, catalog_filter_allpass_inputs, FIELD_COUNT(catalog_filter_allpass_inputs), catalog_filter_allpass_outputs, FIELD_COUNT(catalog_filter_allpass_outputs), catalog_filter_allpass_config, FIELD_COUNT(catalog_filter_allpass_config)},
    {"filter_biquad_coefficients", APG_ATOM_VISIBILITY_ADVANCED, catalog_filter_biquad_coefficients_inputs, FIELD_COUNT(catalog_filter_biquad_coefficients_inputs), catalog_filter_biquad_coefficients_outputs, FIELD_COUNT(catalog_filter_biquad_coefficients_outputs), catalog_filter_biquad_coefficients_config, FIELD_COUNT(catalog_filter_biquad_coefficients_config)},
    {"filter_biquad", APG_ATOM_VISIBILITY_PUBLIC, catalog_filter_biquad_inputs, FIELD_COUNT(catalog_filter_biquad_inputs), catalog_filter_biquad_outputs, FIELD_COUNT(catalog_filter_biquad_outputs), catalog_filter_biquad_config, FIELD_COUNT(catalog_filter_biquad_config)},
    {"filter_comb_fb", APG_ATOM_VISIBILITY_ADVANCED, catalog_filter_comb_fb_inputs, FIELD_COUNT(catalog_filter_comb_fb_inputs), catalog_filter_comb_fb_outputs, FIELD_COUNT(catalog_filter_comb_fb_outputs), catalog_filter_comb_fb_config, FIELD_COUNT(catalog_filter_comb_fb_config)},
    {"filter_comb_ff", APG_ATOM_VISIBILITY_ADVANCED, catalog_filter_comb_ff_inputs, FIELD_COUNT(catalog_filter_comb_ff_inputs), catalog_filter_comb_ff_outputs, FIELD_COUNT(catalog_filter_comb_ff_outputs), catalog_filter_comb_ff_config, FIELD_COUNT(catalog_filter_comb_ff_config)},
    {"filter_dc_block", APG_ATOM_VISIBILITY_PUBLIC, catalog_filter_dc_block_inputs, FIELD_COUNT(catalog_filter_dc_block_inputs), catalog_filter_dc_block_outputs, FIELD_COUNT(catalog_filter_dc_block_outputs), catalog_filter_dc_block_config, FIELD_COUNT(catalog_filter_dc_block_config)},
    {"filter_differentiate", APG_ATOM_VISIBILITY_INTERNAL, catalog_filter_differentiate_inputs, FIELD_COUNT(catalog_filter_differentiate_inputs), catalog_filter_differentiate_outputs, FIELD_COUNT(catalog_filter_differentiate_outputs), NULL, 0u},
    {"filter_fir", APG_ATOM_VISIBILITY_ADVANCED, catalog_filter_fir_inputs, FIELD_COUNT(catalog_filter_fir_inputs), catalog_filter_fir_outputs, FIELD_COUNT(catalog_filter_fir_outputs), catalog_filter_fir_config, FIELD_COUNT(catalog_filter_fir_config)},
    {"filter_integrate", APG_ATOM_VISIBILITY_INTERNAL, catalog_filter_integrate_inputs, FIELD_COUNT(catalog_filter_integrate_inputs), catalog_filter_integrate_outputs, FIELD_COUNT(catalog_filter_integrate_outputs), NULL, 0u},
    {"freq_fft", APG_ATOM_VISIBILITY_ADVANCED, catalog_freq_fft_inputs, FIELD_COUNT(catalog_freq_fft_inputs), catalog_freq_fft_outputs, FIELD_COUNT(catalog_freq_fft_outputs), catalog_freq_fft_config, FIELD_COUNT(catalog_freq_fft_config)},
    {"freq_ifft", APG_ATOM_VISIBILITY_ADVANCED, catalog_freq_ifft_inputs, FIELD_COUNT(catalog_freq_ifft_inputs), catalog_freq_ifft_outputs, FIELD_COUNT(catalog_freq_ifft_outputs), catalog_freq_ifft_config, FIELD_COUNT(catalog_freq_ifft_config)},
    {"freq_multiply", APG_ATOM_VISIBILITY_ADVANCED, catalog_freq_multiply_inputs, FIELD_COUNT(catalog_freq_multiply_inputs), catalog_freq_multiply_outputs, FIELD_COUNT(catalog_freq_multiply_outputs), catalog_freq_multiply_config, FIELD_COUNT(catalog_freq_multiply_config)},
    {"freq_overlap_add", APG_ATOM_VISIBILITY_INTERNAL, catalog_freq_overlap_add_inputs, FIELD_COUNT(catalog_freq_overlap_add_inputs), catalog_freq_overlap_add_outputs, FIELD_COUNT(catalog_freq_overlap_add_outputs), catalog_freq_overlap_add_config, FIELD_COUNT(catalog_freq_overlap_add_config)},
    {"freq_overlap_save", APG_ATOM_VISIBILITY_INTERNAL, catalog_freq_overlap_save_inputs, FIELD_COUNT(catalog_freq_overlap_save_inputs), catalog_freq_overlap_save_outputs, FIELD_COUNT(catalog_freq_overlap_save_outputs), catalog_freq_overlap_save_config, FIELD_COUNT(catalog_freq_overlap_save_config)},
    {"freq_window", APG_ATOM_VISIBILITY_INTERNAL, catalog_freq_window_inputs, FIELD_COUNT(catalog_freq_window_inputs), catalog_freq_window_outputs, FIELD_COUNT(catalog_freq_window_outputs), catalog_freq_window_config, FIELD_COUNT(catalog_freq_window_config)},
    {"freq_shift", APG_ATOM_VISIBILITY_PUBLIC, catalog_freq_shift_inputs, FIELD_COUNT(catalog_freq_shift_inputs), catalog_freq_shift_outputs, FIELD_COUNT(catalog_freq_shift_outputs), catalog_freq_shift_config, FIELD_COUNT(catalog_freq_shift_config)},
    {"generation_dc", APG_ATOM_VISIBILITY_PUBLIC, NULL, 0u, catalog_generation_dc_outputs, FIELD_COUNT(catalog_generation_dc_outputs), catalog_generation_dc_config, FIELD_COUNT(catalog_generation_dc_config)},
    {"generation_envelope", APG_ATOM_VISIBILITY_PUBLIC, catalog_generation_envelope_inputs, FIELD_COUNT(catalog_generation_envelope_inputs), catalog_generation_envelope_outputs, FIELD_COUNT(catalog_generation_envelope_outputs), catalog_generation_envelope_config, FIELD_COUNT(catalog_generation_envelope_config)},
    {"generation_impulse", APG_ATOM_VISIBILITY_PUBLIC, NULL, 0u, catalog_generation_impulse_outputs, FIELD_COUNT(catalog_generation_impulse_outputs), catalog_generation_impulse_config, FIELD_COUNT(catalog_generation_impulse_config)},
    {"generation_lfo", APG_ATOM_VISIBILITY_INTERNAL, NULL, 0u, catalog_generation_lfo_outputs, FIELD_COUNT(catalog_generation_lfo_outputs), catalog_generation_lfo_config, FIELD_COUNT(catalog_generation_lfo_config)},
    {"generation_noise", APG_ATOM_VISIBILITY_PUBLIC, NULL, 0u, catalog_generation_noise_outputs, FIELD_COUNT(catalog_generation_noise_outputs), catalog_generation_noise_config, FIELD_COUNT(catalog_generation_noise_config)},
    {"generation_oscillator", APG_ATOM_VISIBILITY_PUBLIC, catalog_generation_oscillator_inputs, FIELD_COUNT(catalog_generation_oscillator_inputs), catalog_generation_oscillator_outputs, FIELD_COUNT(catalog_generation_oscillator_outputs), catalog_generation_oscillator_config, FIELD_COUNT(catalog_generation_oscillator_config)},
    {"interpolation_cubic", APG_ATOM_VISIBILITY_INTERNAL, catalog_interpolation_cubic_inputs, FIELD_COUNT(catalog_interpolation_cubic_inputs), catalog_interpolation_cubic_outputs, FIELD_COUNT(catalog_interpolation_cubic_outputs), NULL, 0u},
    {"interpolation_lagrange", APG_ATOM_VISIBILITY_INTERNAL, catalog_interpolation_lagrange_inputs, FIELD_COUNT(catalog_interpolation_lagrange_inputs), catalog_interpolation_lagrange_outputs, FIELD_COUNT(catalog_interpolation_lagrange_outputs), catalog_interpolation_lagrange_config, FIELD_COUNT(catalog_interpolation_lagrange_config)},
    {"interpolation_linear", APG_ATOM_VISIBILITY_INTERNAL, catalog_interpolation_linear_inputs, FIELD_COUNT(catalog_interpolation_linear_inputs), catalog_interpolation_linear_outputs, FIELD_COUNT(catalog_interpolation_linear_outputs), NULL, 0u},
    {"interpolation_sinc", APG_ATOM_VISIBILITY_INTERNAL, catalog_interpolation_sinc_inputs, FIELD_COUNT(catalog_interpolation_sinc_inputs), catalog_interpolation_sinc_outputs, FIELD_COUNT(catalog_interpolation_sinc_outputs), catalog_interpolation_sinc_config, FIELD_COUNT(catalog_interpolation_sinc_config)},
    {"mix_crossfade", APG_ATOM_VISIBILITY_PUBLIC, catalog_mix_crossfade_inputs, FIELD_COUNT(catalog_mix_crossfade_inputs), catalog_mix_crossfade_outputs, FIELD_COUNT(catalog_mix_crossfade_outputs), catalog_mix_crossfade_config, FIELD_COUNT(catalog_mix_crossfade_config)},
    {"mix_decode_ms", APG_ATOM_VISIBILITY_ADVANCED, catalog_mix_decode_ms_inputs, FIELD_COUNT(catalog_mix_decode_ms_inputs), catalog_mix_decode_ms_outputs, FIELD_COUNT(catalog_mix_decode_ms_outputs), NULL, 0u},
    {"mix_encode_ms", APG_ATOM_VISIBILITY_ADVANCED, catalog_mix_encode_ms_inputs, FIELD_COUNT(catalog_mix_encode_ms_inputs), catalog_mix_encode_ms_outputs, FIELD_COUNT(catalog_mix_encode_ms_outputs), NULL, 0u},
    {"mix_matrix", APG_ATOM_VISIBILITY_ADVANCED, catalog_mix_matrix_inputs, FIELD_COUNT(catalog_mix_matrix_inputs), catalog_mix_matrix_outputs, FIELD_COUNT(catalog_mix_matrix_outputs), catalog_mix_matrix_config, FIELD_COUNT(catalog_mix_matrix_config)},
    {"mix_pan_stereo", APG_ATOM_VISIBILITY_PUBLIC, catalog_mix_pan_stereo_inputs, FIELD_COUNT(catalog_mix_pan_stereo_inputs), catalog_mix_pan_stereo_outputs, FIELD_COUNT(catalog_mix_pan_stereo_outputs), catalog_mix_pan_stereo_config, FIELD_COUNT(catalog_mix_pan_stereo_config)},
    {"mix_wet_dry", APG_ATOM_VISIBILITY_INTERNAL, catalog_mix_wet_dry_inputs, FIELD_COUNT(catalog_mix_wet_dry_inputs), catalog_mix_wet_dry_outputs, FIELD_COUNT(catalog_mix_wet_dry_outputs), catalog_mix_wet_dry_config, FIELD_COUNT(catalog_mix_wet_dry_config)},
    {"modulation_amplitude", APG_ATOM_VISIBILITY_PUBLIC, catalog_modulation_amplitude_inputs, FIELD_COUNT(catalog_modulation_amplitude_inputs), catalog_modulation_amplitude_outputs, FIELD_COUNT(catalog_modulation_amplitude_outputs), catalog_modulation_amplitude_config, FIELD_COUNT(catalog_modulation_amplitude_config)},
    {"modulation_frequency", APG_ATOM_VISIBILITY_ADVANCED, catalog_modulation_frequency_inputs, FIELD_COUNT(catalog_modulation_frequency_inputs), catalog_modulation_frequency_outputs, FIELD_COUNT(catalog_modulation_frequency_outputs), catalog_modulation_frequency_config, FIELD_COUNT(catalog_modulation_frequency_config)},
    {"modulation_phase", APG_ATOM_VISIBILITY_ADVANCED, catalog_modulation_phase_inputs, FIELD_COUNT(catalog_modulation_phase_inputs), catalog_modulation_phase_outputs, FIELD_COUNT(catalog_modulation_phase_outputs), catalog_modulation_phase_config, FIELD_COUNT(catalog_modulation_phase_config)},
    {"modulation_phaser", APG_ATOM_VISIBILITY_ADVANCED, catalog_modulation_phaser_inputs, FIELD_COUNT(catalog_modulation_phaser_inputs), catalog_modulation_phaser_outputs, FIELD_COUNT(catalog_modulation_phaser_outputs), catalog_modulation_phaser_config, FIELD_COUNT(catalog_modulation_phaser_config)},
    {"modulation_ring", APG_ATOM_VISIBILITY_PUBLIC, catalog_modulation_ring_inputs, FIELD_COUNT(catalog_modulation_ring_inputs), catalog_modulation_ring_outputs, FIELD_COUNT(catalog_modulation_ring_outputs), NULL, 0u},
    {"modulation_scrub", APG_ATOM_VISIBILITY_ADVANCED, catalog_modulation_scrub_inputs, FIELD_COUNT(catalog_modulation_scrub_inputs), catalog_modulation_scrub_outputs, FIELD_COUNT(catalog_modulation_scrub_outputs), catalog_modulation_scrub_config, FIELD_COUNT(catalog_modulation_scrub_config)},
    {"nonlinear_bitcrush", APG_ATOM_VISIBILITY_PUBLIC, catalog_nonlinear_bitcrush_inputs, FIELD_COUNT(catalog_nonlinear_bitcrush_inputs), catalog_nonlinear_bitcrush_outputs, FIELD_COUNT(catalog_nonlinear_bitcrush_outputs), catalog_nonlinear_bitcrush_config, FIELD_COUNT(catalog_nonlinear_bitcrush_config)},
    {"nonlinear_sample_hold", APG_ATOM_VISIBILITY_PUBLIC, catalog_nonlinear_sample_hold_inputs, FIELD_COUNT(catalog_nonlinear_sample_hold_inputs), catalog_nonlinear_sample_hold_outputs, FIELD_COUNT(catalog_nonlinear_sample_hold_outputs), catalog_nonlinear_sample_hold_config, FIELD_COUNT(catalog_nonlinear_sample_hold_config)},
    {"nonlinear_waveshape", APG_ATOM_VISIBILITY_PUBLIC, catalog_nonlinear_waveshape_inputs, FIELD_COUNT(catalog_nonlinear_waveshape_inputs), catalog_nonlinear_waveshape_outputs, FIELD_COUNT(catalog_nonlinear_waveshape_outputs), catalog_nonlinear_waveshape_config, FIELD_COUNT(catalog_nonlinear_waveshape_config)},
    {"src_antialias", APG_ATOM_VISIBILITY_INTERNAL, catalog_src_antialias_inputs, FIELD_COUNT(catalog_src_antialias_inputs), catalog_src_antialias_outputs, FIELD_COUNT(catalog_src_antialias_outputs), catalog_src_antialias_config, FIELD_COUNT(catalog_src_antialias_config)},
    {"src_antiimage", APG_ATOM_VISIBILITY_INTERNAL, catalog_src_antiimage_inputs, FIELD_COUNT(catalog_src_antiimage_inputs), catalog_src_antiimage_outputs, FIELD_COUNT(catalog_src_antiimage_outputs), catalog_src_antiimage_config, FIELD_COUNT(catalog_src_antiimage_config)},
    {"src_convert_format", APG_ATOM_VISIBILITY_ADVANCED, catalog_src_convert_format_inputs, FIELD_COUNT(catalog_src_convert_format_inputs), catalog_src_convert_format_outputs, FIELD_COUNT(catalog_src_convert_format_outputs), catalog_src_convert_format_config, FIELD_COUNT(catalog_src_convert_format_config)},
    {"src_downsample", APG_ATOM_VISIBILITY_INTERNAL, catalog_src_downsample_inputs, FIELD_COUNT(catalog_src_downsample_inputs), catalog_src_downsample_outputs, FIELD_COUNT(catalog_src_downsample_outputs), catalog_src_downsample_config, FIELD_COUNT(catalog_src_downsample_config)},
    {"src_upsample", APG_ATOM_VISIBILITY_INTERNAL, catalog_src_upsample_inputs, FIELD_COUNT(catalog_src_upsample_inputs), catalog_src_upsample_outputs, FIELD_COUNT(catalog_src_upsample_outputs), catalog_src_upsample_config, FIELD_COUNT(catalog_src_upsample_config)},
    {"freq_quantize", APG_ATOM_VISIBILITY_ADVANCED, catalog_freq_quantize_inputs, FIELD_COUNT(catalog_freq_quantize_inputs), catalog_freq_quantize_outputs, FIELD_COUNT(catalog_freq_quantize_outputs), NULL, 0u},
    {"math_difference", APG_ATOM_VISIBILITY_ADVANCED, catalog_math_difference_inputs, FIELD_COUNT(catalog_math_difference_inputs), catalog_math_difference_outputs, FIELD_COUNT(catalog_math_difference_outputs), NULL, 0u},
    {"math_integrate", APG_ATOM_VISIBILITY_ADVANCED, catalog_math_integrate_inputs, FIELD_COUNT(catalog_math_integrate_inputs), catalog_math_integrate_outputs, FIELD_COUNT(catalog_math_integrate_outputs), catalog_math_integrate_config, FIELD_COUNT(catalog_math_integrate_config)},
};
