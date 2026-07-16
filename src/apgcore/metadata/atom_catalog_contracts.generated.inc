/* Generated from schema/atoms/atoms.json by tools/generate_atom_artifacts.pl. Do not edit. */

static const apg_atom_contract_field_t catalog_amplitude_accumulate_inputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_amplitude_accumulate_outputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_amplitude_latch_inputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
    FIELD("gate", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_amplitude_latch_outputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_amplitude_latch_config[] = {
    FIELD("threshold", APG_ATOM_FIELD_FLOAT),
};

static const apg_atom_contract_field_t catalog_amplitude_add_inputs[] = {
    FIELD("signal_a", APG_ATOM_FIELD_SIGNAL),
    FIELD("signal_b", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_amplitude_add_outputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_amplitude_clip_hard_inputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_amplitude_clip_hard_outputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_amplitude_clip_hard_config[] = {
    FIELD("threshold", APG_ATOM_FIELD_FLOAT),
};

static const apg_atom_contract_field_t catalog_amplitude_clip_soft_inputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_amplitude_clip_soft_outputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_amplitude_clip_soft_config[] = {
    FIELD("threshold", APG_ATOM_FIELD_FLOAT),
    FIELD("curve", APG_ATOM_FIELD_INT),
};

static const apg_atom_contract_field_t catalog_amplitude_divide_inputs[] = {
    FIELD("numerator", APG_ATOM_FIELD_SIGNAL),
    FIELD("denominator", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_amplitude_divide_outputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_amplitude_divide_config[] = {
    FIELD("epsilon", APG_ATOM_FIELD_FLOAT),
};

static const apg_atom_contract_field_t catalog_amplitude_multiply_inputs[] = {
    FIELD("signal_a", APG_ATOM_FIELD_SIGNAL),
    FIELD("signal_b", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_amplitude_multiply_outputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_amplitude_normalize_inputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_amplitude_normalize_outputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_amplitude_normalize_config[] = {
    FIELD("target_level", APG_ATOM_FIELD_FLOAT),
    FIELD("mode", APG_ATOM_FIELD_INT),
};

static const apg_atom_contract_field_t catalog_amplitude_smooth_inputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_amplitude_smooth_outputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_amplitude_smooth_config[] = {
    FIELD("attack", APG_ATOM_FIELD_FLOAT),
    FIELD("release", APG_ATOM_FIELD_FLOAT),
};

static const apg_atom_contract_field_t catalog_amplitude_subtract_inputs[] = {
    FIELD("signal_a", APG_ATOM_FIELD_SIGNAL),
    FIELD("signal_b", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_amplitude_subtract_outputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_delay_fractional_inputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_delay_fractional_outputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_delay_fractional_config[] = {
    FIELD("delay_samples", APG_ATOM_FIELD_FLOAT),
    FIELD("interpolation", APG_ATOM_FIELD_INT),
};

static const apg_atom_contract_field_t catalog_delay_line_inputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_delay_line_outputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_delay_line_config[] = {
    FIELD("length", APG_ATOM_FIELD_INT),
};

static const apg_atom_contract_field_t catalog_delay_tap_feedback_inputs[] = {
    FIELD("buffer", APG_ATOM_FIELD_BUFFER),
    FIELD("tap_position", APG_ATOM_FIELD_SCALAR),
};

static const apg_atom_contract_field_t catalog_delay_tap_feedback_outputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_delay_tap_feedback_config[] = {
    FIELD("coefficient", APG_ATOM_FIELD_FLOAT),
};

static const apg_atom_contract_field_t catalog_delay_tap_feedforward_inputs[] = {
    FIELD("buffer", APG_ATOM_FIELD_BUFFER),
    FIELD("tap_position", APG_ATOM_FIELD_SCALAR),
};

static const apg_atom_contract_field_t catalog_delay_tap_feedforward_outputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_delay_tap_feedforward_config[] = {
    FIELD("coefficient", APG_ATOM_FIELD_FLOAT),
};

static const apg_atom_contract_field_t catalog_delay_unit_inputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_delay_unit_outputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_detect_autocorrelate_inputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_detect_autocorrelate_outputs[] = {
    FIELD("correlation", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_detect_autocorrelate_config[] = {
    FIELD("max_lag", APG_ATOM_FIELD_INT),
};

static const apg_atom_contract_field_t catalog_detect_pitch_inputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_detect_pitch_outputs[] = {
    FIELD("pitch", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_detect_pitch_config[] = {
    FIELD("max_lag", APG_ATOM_FIELD_INT),
};

static const apg_atom_contract_field_t catalog_detect_envelope_inputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_detect_envelope_outputs[] = {
    FIELD("envelope", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_detect_envelope_config[] = {
    FIELD("attack", APG_ATOM_FIELD_FLOAT),
    FIELD("release", APG_ATOM_FIELD_FLOAT),
};

static const apg_atom_contract_field_t catalog_detect_peak_inputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_detect_peak_outputs[] = {
    FIELD("level", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_detect_peak_config[] = {
    FIELD("attack", APG_ATOM_FIELD_FLOAT),
    FIELD("release", APG_ATOM_FIELD_FLOAT),
};

static const apg_atom_contract_field_t catalog_detect_rms_inputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_detect_rms_outputs[] = {
    FIELD("level", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_detect_rms_config[] = {
    FIELD("window_size", APG_ATOM_FIELD_INT),
};

static const apg_atom_contract_field_t catalog_detect_slope_inputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_detect_slope_outputs[] = {
    FIELD("slope", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_detect_threshold_inputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_detect_threshold_outputs[] = {
    FIELD("gate", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_detect_threshold_config[] = {
    FIELD("threshold", APG_ATOM_FIELD_FLOAT),
};

static const apg_atom_contract_field_t catalog_detect_zero_crossing_inputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_detect_zero_crossing_outputs[] = {
    FIELD("trigger", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_filter_allpass_inputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_filter_allpass_outputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_filter_allpass_config[] = {
    FIELD("delay_samples", APG_ATOM_FIELD_INT),
    FIELD("coefficient", APG_ATOM_FIELD_FLOAT),
};

static const apg_atom_contract_field_t catalog_filter_biquad_coefficients_inputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_filter_biquad_coefficients_outputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_filter_biquad_coefficients_config[] = {
    FIELD("b0", APG_ATOM_FIELD_FLOAT),
    FIELD("b1", APG_ATOM_FIELD_FLOAT),
    FIELD("b2", APG_ATOM_FIELD_FLOAT),
    FIELD("a1", APG_ATOM_FIELD_FLOAT),
    FIELD("a2", APG_ATOM_FIELD_FLOAT),
};

static const apg_atom_contract_field_t catalog_filter_biquad_inputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
    FIELD_OPT("cutoff", APG_ATOM_FIELD_SIGNAL_OPTIONAL),
};

static const apg_atom_contract_field_t catalog_filter_biquad_outputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_filter_biquad_config[] = {
    FIELD("cutoff", APG_ATOM_FIELD_FLOAT),
    FIELD("q", APG_ATOM_FIELD_FLOAT),
    FIELD("mode", APG_ATOM_FIELD_INT),
    FIELD("smoothing_ms", APG_ATOM_FIELD_FLOAT),
};

static const apg_atom_contract_field_t catalog_filter_comb_fb_inputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
    FIELD_OPT("delay", APG_ATOM_FIELD_SIGNAL_OPTIONAL),
};

static const apg_atom_contract_field_t catalog_filter_comb_fb_outputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_filter_comb_fb_config[] = {
    FIELD("delay_samples", APG_ATOM_FIELD_INT),
    FIELD("coefficient", APG_ATOM_FIELD_FLOAT),
};

static const apg_atom_contract_field_t catalog_filter_comb_ff_inputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_filter_comb_ff_outputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_filter_comb_ff_config[] = {
    FIELD("delay_samples", APG_ATOM_FIELD_INT),
    FIELD("coefficient", APG_ATOM_FIELD_FLOAT),
};

static const apg_atom_contract_field_t catalog_filter_dc_block_inputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_filter_dc_block_outputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_filter_dc_block_config[] = {
    FIELD("coefficient", APG_ATOM_FIELD_FLOAT),
};

static const apg_atom_contract_field_t catalog_filter_differentiate_inputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_filter_differentiate_outputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_filter_fir_inputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_filter_fir_outputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_filter_fir_config[] = {
    FIELD("kernel", APG_ATOM_FIELD_BUFFER),
    FIELD("kernel_size", APG_ATOM_FIELD_INT),
};

static const apg_atom_contract_field_t catalog_filter_integrate_inputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_filter_integrate_outputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_freq_fft_inputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_freq_fft_outputs[] = {
    FIELD("real", APG_ATOM_FIELD_SIGNAL),
    FIELD("imag", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_freq_fft_config[] = {
    FIELD("block_size", APG_ATOM_FIELD_INT),
};

static const apg_atom_contract_field_t catalog_freq_ifft_inputs[] = {
    FIELD("real", APG_ATOM_FIELD_SIGNAL),
    FIELD("imag", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_freq_ifft_outputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_freq_ifft_config[] = {
    FIELD("block_size", APG_ATOM_FIELD_INT),
};

static const apg_atom_contract_field_t catalog_freq_multiply_inputs[] = {
    FIELD("real_a", APG_ATOM_FIELD_SIGNAL),
    FIELD("imag_a", APG_ATOM_FIELD_SIGNAL),
    FIELD("real_b", APG_ATOM_FIELD_SIGNAL),
    FIELD("imag_b", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_freq_multiply_outputs[] = {
    FIELD("real", APG_ATOM_FIELD_SIGNAL),
    FIELD("imag", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_freq_multiply_config[] = {
    FIELD("block_size", APG_ATOM_FIELD_INT),
};

static const apg_atom_contract_field_t catalog_freq_overlap_add_inputs[] = {
    FIELD("frame", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_freq_overlap_add_outputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_freq_overlap_add_config[] = {
    FIELD("block_size", APG_ATOM_FIELD_INT),
    FIELD("hop_size", APG_ATOM_FIELD_INT),
};

static const apg_atom_contract_field_t catalog_freq_overlap_save_inputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_freq_overlap_save_outputs[] = {
    FIELD("frame", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_freq_overlap_save_config[] = {
    FIELD("block_size", APG_ATOM_FIELD_INT),
    FIELD("hop_size", APG_ATOM_FIELD_INT),
};

static const apg_atom_contract_field_t catalog_freq_window_inputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_freq_window_outputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_freq_window_config[] = {
    FIELD("window_type", APG_ATOM_FIELD_INT),
    FIELD("block_size", APG_ATOM_FIELD_INT),
};

static const apg_atom_contract_field_t catalog_freq_shift_inputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
    FIELD_OPT("pitch_shift", APG_ATOM_FIELD_SIGNAL_OPTIONAL),
};

static const apg_atom_contract_field_t catalog_freq_shift_outputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_freq_shift_config[] = {
    FIELD("block_size", APG_ATOM_FIELD_INT),
};

static const apg_atom_contract_field_t catalog_generation_dc_outputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_generation_dc_config[] = {
    FIELD("value", APG_ATOM_FIELD_SCALAR),
};

static const apg_atom_contract_field_t catalog_generation_envelope_inputs[] = {
    FIELD("gate", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_generation_envelope_outputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_generation_envelope_config[] = {
    FIELD("attack", APG_ATOM_FIELD_FLOAT),
    FIELD("decay", APG_ATOM_FIELD_FLOAT),
    FIELD("sustain", APG_ATOM_FIELD_FLOAT),
    FIELD("release", APG_ATOM_FIELD_FLOAT),
};

static const apg_atom_contract_field_t catalog_generation_impulse_outputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_generation_impulse_config[] = {
    FIELD("interval", APG_ATOM_FIELD_FLOAT),
};

static const apg_atom_contract_field_t catalog_generation_lfo_outputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_generation_lfo_config[] = {
    FIELD("frequency", APG_ATOM_FIELD_FLOAT),
    FIELD("waveform", APG_ATOM_FIELD_INT),
    FIELD("phase_offset", APG_ATOM_FIELD_FLOAT),
};

static const apg_atom_contract_field_t catalog_generation_noise_outputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_generation_noise_config[] = {
    FIELD("amplitude", APG_ATOM_FIELD_FLOAT),
    FIELD("color", APG_ATOM_FIELD_INT),
};

static const apg_atom_contract_field_t catalog_generation_oscillator_inputs[] = {
    FIELD_OPT("frequency", APG_ATOM_FIELD_SIGNAL_OPTIONAL),
};

static const apg_atom_contract_field_t catalog_generation_oscillator_outputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_generation_oscillator_config[] = {
    FIELD("frequency", APG_ATOM_FIELD_FLOAT),
    FIELD("waveform", APG_ATOM_FIELD_INT),
    FIELD("phase_offset", APG_ATOM_FIELD_FLOAT),
};

static const apg_atom_contract_field_t catalog_interpolation_cubic_inputs[] = {
    FIELD("signal_n1", APG_ATOM_FIELD_SIGNAL),
    FIELD("signal_a", APG_ATOM_FIELD_SIGNAL),
    FIELD("signal_b", APG_ATOM_FIELD_SIGNAL),
    FIELD("signal_c", APG_ATOM_FIELD_SIGNAL),
    FIELD("t", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_interpolation_cubic_outputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_interpolation_lagrange_inputs[] = {
    FIELD("samples", APG_ATOM_FIELD_SIGNAL),
    FIELD("t", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_interpolation_lagrange_outputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_interpolation_lagrange_config[] = {
    FIELD("order", APG_ATOM_FIELD_INT),
};

static const apg_atom_contract_field_t catalog_interpolation_linear_inputs[] = {
    FIELD("signal_a", APG_ATOM_FIELD_SIGNAL),
    FIELD("signal_b", APG_ATOM_FIELD_SIGNAL),
    FIELD("t", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_interpolation_linear_outputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_interpolation_sinc_inputs[] = {
    FIELD("buffer", APG_ATOM_FIELD_BUFFER),
    FIELD("position", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_interpolation_sinc_outputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_interpolation_sinc_config[] = {
    FIELD("num_taps", APG_ATOM_FIELD_INT),
};

static const apg_atom_contract_field_t catalog_mix_crossfade_inputs[] = {
    FIELD("signal_a", APG_ATOM_FIELD_SIGNAL),
    FIELD("signal_b", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_mix_crossfade_outputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_mix_crossfade_config[] = {
    FIELD("t", APG_ATOM_FIELD_FLOAT),
};

static const apg_atom_contract_field_t catalog_mix_decode_ms_inputs[] = {
    FIELD("mid", APG_ATOM_FIELD_SIGNAL),
    FIELD("side", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_mix_decode_ms_outputs[] = {
    FIELD("left", APG_ATOM_FIELD_SIGNAL),
    FIELD("right", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_mix_encode_ms_inputs[] = {
    FIELD("left", APG_ATOM_FIELD_SIGNAL),
    FIELD("right", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_mix_encode_ms_outputs[] = {
    FIELD("mid", APG_ATOM_FIELD_SIGNAL),
    FIELD("side", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_mix_matrix_inputs[] = {
    FIELD("signals", APG_ATOM_FIELD_SIGNAL_ARRAY),
};

static const apg_atom_contract_field_t catalog_mix_matrix_outputs[] = {
    FIELD("signals", APG_ATOM_FIELD_SIGNAL_ARRAY),
};

static const apg_atom_contract_field_t catalog_mix_matrix_config[] = {
    FIELD("coefficients", APG_ATOM_FIELD_FLOAT_MATRIX),
};

static const apg_atom_contract_field_t catalog_mix_pan_stereo_inputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_mix_pan_stereo_outputs[] = {
    FIELD("left", APG_ATOM_FIELD_SIGNAL),
    FIELD("right", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_mix_pan_stereo_config[] = {
    FIELD("position", APG_ATOM_FIELD_FLOAT),
};

static const apg_atom_contract_field_t catalog_mix_wet_dry_inputs[] = {
    FIELD("dry", APG_ATOM_FIELD_SIGNAL),
    FIELD("wet", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_mix_wet_dry_outputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_mix_wet_dry_config[] = {
    FIELD("mix", APG_ATOM_FIELD_FLOAT),
};

static const apg_atom_contract_field_t catalog_modulation_amplitude_inputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
    FIELD("modulator", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_modulation_amplitude_outputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_modulation_amplitude_config[] = {
    FIELD("depth", APG_ATOM_FIELD_FLOAT),
};

static const apg_atom_contract_field_t catalog_modulation_frequency_inputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
    FIELD("modulator", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_modulation_frequency_outputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_modulation_frequency_config[] = {
    FIELD("depth", APG_ATOM_FIELD_FLOAT),
};

static const apg_atom_contract_field_t catalog_modulation_phase_inputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
    FIELD("modulator", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_modulation_phase_outputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_modulation_phase_config[] = {
    FIELD("depth", APG_ATOM_FIELD_FLOAT),
};

static const apg_atom_contract_field_t catalog_modulation_ring_inputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
    FIELD("modulator", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_modulation_ring_outputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_modulation_scrub_inputs[] = {
    FIELD("buffer", APG_ATOM_FIELD_BUFFER),
    FIELD("position", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_modulation_scrub_outputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_modulation_scrub_config[] = {
    FIELD("buffer_size", APG_ATOM_FIELD_INT),
};

static const apg_atom_contract_field_t catalog_nonlinear_bitcrush_inputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_nonlinear_bitcrush_outputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_nonlinear_bitcrush_config[] = {
    FIELD("bit_depth", APG_ATOM_FIELD_FLOAT),
};

static const apg_atom_contract_field_t catalog_nonlinear_sample_hold_inputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_nonlinear_sample_hold_outputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_nonlinear_sample_hold_config[] = {
    FIELD("factor", APG_ATOM_FIELD_FLOAT),
};

static const apg_atom_contract_field_t catalog_nonlinear_waveshape_inputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_nonlinear_waveshape_outputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_nonlinear_waveshape_config[] = {
    FIELD("transfer_table", APG_ATOM_FIELD_BUFFER),
    FIELD("table_size", APG_ATOM_FIELD_INT),
};

static const apg_atom_contract_field_t catalog_src_antialias_inputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_src_antialias_outputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_src_antialias_config[] = {
    FIELD("cutoff", APG_ATOM_FIELD_FLOAT),
};

static const apg_atom_contract_field_t catalog_src_antiimage_inputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_src_antiimage_outputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_src_antiimage_config[] = {
    FIELD("cutoff", APG_ATOM_FIELD_FLOAT),
};

static const apg_atom_contract_field_t catalog_src_convert_format_inputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_src_convert_format_outputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_src_convert_format_config[] = {
    FIELD("from_format", APG_ATOM_FIELD_INT),
    FIELD("to_format", APG_ATOM_FIELD_INT),
};

static const apg_atom_contract_field_t catalog_src_downsample_inputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_src_downsample_outputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_src_downsample_config[] = {
    FIELD("factor", APG_ATOM_FIELD_INT),
};

static const apg_atom_contract_field_t catalog_src_upsample_inputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_src_upsample_outputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_src_upsample_config[] = {
    FIELD("factor", APG_ATOM_FIELD_INT),
};

static const apg_atom_contract_field_t catalog_freq_quantize_inputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_atom_contract_field_t catalog_freq_quantize_outputs[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};

static const apg_catalog_contract_t catalog_contracts[] = {
    {"amplitude_accumulate", catalog_amplitude_accumulate_inputs, FIELD_COUNT(catalog_amplitude_accumulate_inputs), catalog_amplitude_accumulate_outputs, FIELD_COUNT(catalog_amplitude_accumulate_outputs), NULL, 0u},
    {"amplitude_latch", catalog_amplitude_latch_inputs, FIELD_COUNT(catalog_amplitude_latch_inputs), catalog_amplitude_latch_outputs, FIELD_COUNT(catalog_amplitude_latch_outputs), catalog_amplitude_latch_config, FIELD_COUNT(catalog_amplitude_latch_config)},
    {"amplitude_add", catalog_amplitude_add_inputs, FIELD_COUNT(catalog_amplitude_add_inputs), catalog_amplitude_add_outputs, FIELD_COUNT(catalog_amplitude_add_outputs), NULL, 0u},
    {"amplitude_clip_hard", catalog_amplitude_clip_hard_inputs, FIELD_COUNT(catalog_amplitude_clip_hard_inputs), catalog_amplitude_clip_hard_outputs, FIELD_COUNT(catalog_amplitude_clip_hard_outputs), catalog_amplitude_clip_hard_config, FIELD_COUNT(catalog_amplitude_clip_hard_config)},
    {"amplitude_clip_soft", catalog_amplitude_clip_soft_inputs, FIELD_COUNT(catalog_amplitude_clip_soft_inputs), catalog_amplitude_clip_soft_outputs, FIELD_COUNT(catalog_amplitude_clip_soft_outputs), catalog_amplitude_clip_soft_config, FIELD_COUNT(catalog_amplitude_clip_soft_config)},
    {"amplitude_divide", catalog_amplitude_divide_inputs, FIELD_COUNT(catalog_amplitude_divide_inputs), catalog_amplitude_divide_outputs, FIELD_COUNT(catalog_amplitude_divide_outputs), catalog_amplitude_divide_config, FIELD_COUNT(catalog_amplitude_divide_config)},
    {"amplitude_multiply", catalog_amplitude_multiply_inputs, FIELD_COUNT(catalog_amplitude_multiply_inputs), catalog_amplitude_multiply_outputs, FIELD_COUNT(catalog_amplitude_multiply_outputs), NULL, 0u},
    {"amplitude_normalize", catalog_amplitude_normalize_inputs, FIELD_COUNT(catalog_amplitude_normalize_inputs), catalog_amplitude_normalize_outputs, FIELD_COUNT(catalog_amplitude_normalize_outputs), catalog_amplitude_normalize_config, FIELD_COUNT(catalog_amplitude_normalize_config)},
    {"amplitude_smooth", catalog_amplitude_smooth_inputs, FIELD_COUNT(catalog_amplitude_smooth_inputs), catalog_amplitude_smooth_outputs, FIELD_COUNT(catalog_amplitude_smooth_outputs), catalog_amplitude_smooth_config, FIELD_COUNT(catalog_amplitude_smooth_config)},
    {"amplitude_subtract", catalog_amplitude_subtract_inputs, FIELD_COUNT(catalog_amplitude_subtract_inputs), catalog_amplitude_subtract_outputs, FIELD_COUNT(catalog_amplitude_subtract_outputs), NULL, 0u},
    {"delay_fractional", catalog_delay_fractional_inputs, FIELD_COUNT(catalog_delay_fractional_inputs), catalog_delay_fractional_outputs, FIELD_COUNT(catalog_delay_fractional_outputs), catalog_delay_fractional_config, FIELD_COUNT(catalog_delay_fractional_config)},
    {"delay_line", catalog_delay_line_inputs, FIELD_COUNT(catalog_delay_line_inputs), catalog_delay_line_outputs, FIELD_COUNT(catalog_delay_line_outputs), catalog_delay_line_config, FIELD_COUNT(catalog_delay_line_config)},
    {"delay_tap_feedback", catalog_delay_tap_feedback_inputs, FIELD_COUNT(catalog_delay_tap_feedback_inputs), catalog_delay_tap_feedback_outputs, FIELD_COUNT(catalog_delay_tap_feedback_outputs), catalog_delay_tap_feedback_config, FIELD_COUNT(catalog_delay_tap_feedback_config)},
    {"delay_tap_feedforward", catalog_delay_tap_feedforward_inputs, FIELD_COUNT(catalog_delay_tap_feedforward_inputs), catalog_delay_tap_feedforward_outputs, FIELD_COUNT(catalog_delay_tap_feedforward_outputs), catalog_delay_tap_feedforward_config, FIELD_COUNT(catalog_delay_tap_feedforward_config)},
    {"delay_unit", catalog_delay_unit_inputs, FIELD_COUNT(catalog_delay_unit_inputs), catalog_delay_unit_outputs, FIELD_COUNT(catalog_delay_unit_outputs), NULL, 0u},
    {"detect_autocorrelate", catalog_detect_autocorrelate_inputs, FIELD_COUNT(catalog_detect_autocorrelate_inputs), catalog_detect_autocorrelate_outputs, FIELD_COUNT(catalog_detect_autocorrelate_outputs), catalog_detect_autocorrelate_config, FIELD_COUNT(catalog_detect_autocorrelate_config)},
    {"detect_pitch", catalog_detect_pitch_inputs, FIELD_COUNT(catalog_detect_pitch_inputs), catalog_detect_pitch_outputs, FIELD_COUNT(catalog_detect_pitch_outputs), catalog_detect_pitch_config, FIELD_COUNT(catalog_detect_pitch_config)},
    {"detect_envelope", catalog_detect_envelope_inputs, FIELD_COUNT(catalog_detect_envelope_inputs), catalog_detect_envelope_outputs, FIELD_COUNT(catalog_detect_envelope_outputs), catalog_detect_envelope_config, FIELD_COUNT(catalog_detect_envelope_config)},
    {"detect_peak", catalog_detect_peak_inputs, FIELD_COUNT(catalog_detect_peak_inputs), catalog_detect_peak_outputs, FIELD_COUNT(catalog_detect_peak_outputs), catalog_detect_peak_config, FIELD_COUNT(catalog_detect_peak_config)},
    {"detect_rms", catalog_detect_rms_inputs, FIELD_COUNT(catalog_detect_rms_inputs), catalog_detect_rms_outputs, FIELD_COUNT(catalog_detect_rms_outputs), catalog_detect_rms_config, FIELD_COUNT(catalog_detect_rms_config)},
    {"detect_slope", catalog_detect_slope_inputs, FIELD_COUNT(catalog_detect_slope_inputs), catalog_detect_slope_outputs, FIELD_COUNT(catalog_detect_slope_outputs), NULL, 0u},
    {"detect_threshold", catalog_detect_threshold_inputs, FIELD_COUNT(catalog_detect_threshold_inputs), catalog_detect_threshold_outputs, FIELD_COUNT(catalog_detect_threshold_outputs), catalog_detect_threshold_config, FIELD_COUNT(catalog_detect_threshold_config)},
    {"detect_zero_crossing", catalog_detect_zero_crossing_inputs, FIELD_COUNT(catalog_detect_zero_crossing_inputs), catalog_detect_zero_crossing_outputs, FIELD_COUNT(catalog_detect_zero_crossing_outputs), NULL, 0u},
    {"filter_allpass", catalog_filter_allpass_inputs, FIELD_COUNT(catalog_filter_allpass_inputs), catalog_filter_allpass_outputs, FIELD_COUNT(catalog_filter_allpass_outputs), catalog_filter_allpass_config, FIELD_COUNT(catalog_filter_allpass_config)},
    {"filter_biquad_coefficients", catalog_filter_biquad_coefficients_inputs, FIELD_COUNT(catalog_filter_biquad_coefficients_inputs), catalog_filter_biquad_coefficients_outputs, FIELD_COUNT(catalog_filter_biquad_coefficients_outputs), catalog_filter_biquad_coefficients_config, FIELD_COUNT(catalog_filter_biquad_coefficients_config)},
    {"filter_biquad", catalog_filter_biquad_inputs, FIELD_COUNT(catalog_filter_biquad_inputs), catalog_filter_biquad_outputs, FIELD_COUNT(catalog_filter_biquad_outputs), catalog_filter_biquad_config, FIELD_COUNT(catalog_filter_biquad_config)},
    {"filter_comb_fb", catalog_filter_comb_fb_inputs, FIELD_COUNT(catalog_filter_comb_fb_inputs), catalog_filter_comb_fb_outputs, FIELD_COUNT(catalog_filter_comb_fb_outputs), catalog_filter_comb_fb_config, FIELD_COUNT(catalog_filter_comb_fb_config)},
    {"filter_comb_ff", catalog_filter_comb_ff_inputs, FIELD_COUNT(catalog_filter_comb_ff_inputs), catalog_filter_comb_ff_outputs, FIELD_COUNT(catalog_filter_comb_ff_outputs), catalog_filter_comb_ff_config, FIELD_COUNT(catalog_filter_comb_ff_config)},
    {"filter_dc_block", catalog_filter_dc_block_inputs, FIELD_COUNT(catalog_filter_dc_block_inputs), catalog_filter_dc_block_outputs, FIELD_COUNT(catalog_filter_dc_block_outputs), catalog_filter_dc_block_config, FIELD_COUNT(catalog_filter_dc_block_config)},
    {"filter_differentiate", catalog_filter_differentiate_inputs, FIELD_COUNT(catalog_filter_differentiate_inputs), catalog_filter_differentiate_outputs, FIELD_COUNT(catalog_filter_differentiate_outputs), NULL, 0u},
    {"filter_fir", catalog_filter_fir_inputs, FIELD_COUNT(catalog_filter_fir_inputs), catalog_filter_fir_outputs, FIELD_COUNT(catalog_filter_fir_outputs), catalog_filter_fir_config, FIELD_COUNT(catalog_filter_fir_config)},
    {"filter_integrate", catalog_filter_integrate_inputs, FIELD_COUNT(catalog_filter_integrate_inputs), catalog_filter_integrate_outputs, FIELD_COUNT(catalog_filter_integrate_outputs), NULL, 0u},
    {"freq_fft", catalog_freq_fft_inputs, FIELD_COUNT(catalog_freq_fft_inputs), catalog_freq_fft_outputs, FIELD_COUNT(catalog_freq_fft_outputs), catalog_freq_fft_config, FIELD_COUNT(catalog_freq_fft_config)},
    {"freq_ifft", catalog_freq_ifft_inputs, FIELD_COUNT(catalog_freq_ifft_inputs), catalog_freq_ifft_outputs, FIELD_COUNT(catalog_freq_ifft_outputs), catalog_freq_ifft_config, FIELD_COUNT(catalog_freq_ifft_config)},
    {"freq_multiply", catalog_freq_multiply_inputs, FIELD_COUNT(catalog_freq_multiply_inputs), catalog_freq_multiply_outputs, FIELD_COUNT(catalog_freq_multiply_outputs), catalog_freq_multiply_config, FIELD_COUNT(catalog_freq_multiply_config)},
    {"freq_overlap_add", catalog_freq_overlap_add_inputs, FIELD_COUNT(catalog_freq_overlap_add_inputs), catalog_freq_overlap_add_outputs, FIELD_COUNT(catalog_freq_overlap_add_outputs), catalog_freq_overlap_add_config, FIELD_COUNT(catalog_freq_overlap_add_config)},
    {"freq_overlap_save", catalog_freq_overlap_save_inputs, FIELD_COUNT(catalog_freq_overlap_save_inputs), catalog_freq_overlap_save_outputs, FIELD_COUNT(catalog_freq_overlap_save_outputs), catalog_freq_overlap_save_config, FIELD_COUNT(catalog_freq_overlap_save_config)},
    {"freq_window", catalog_freq_window_inputs, FIELD_COUNT(catalog_freq_window_inputs), catalog_freq_window_outputs, FIELD_COUNT(catalog_freq_window_outputs), catalog_freq_window_config, FIELD_COUNT(catalog_freq_window_config)},
    {"freq_shift", catalog_freq_shift_inputs, FIELD_COUNT(catalog_freq_shift_inputs), catalog_freq_shift_outputs, FIELD_COUNT(catalog_freq_shift_outputs), catalog_freq_shift_config, FIELD_COUNT(catalog_freq_shift_config)},
    {"generation_dc", NULL, 0u, catalog_generation_dc_outputs, FIELD_COUNT(catalog_generation_dc_outputs), catalog_generation_dc_config, FIELD_COUNT(catalog_generation_dc_config)},
    {"generation_envelope", catalog_generation_envelope_inputs, FIELD_COUNT(catalog_generation_envelope_inputs), catalog_generation_envelope_outputs, FIELD_COUNT(catalog_generation_envelope_outputs), catalog_generation_envelope_config, FIELD_COUNT(catalog_generation_envelope_config)},
    {"generation_impulse", NULL, 0u, catalog_generation_impulse_outputs, FIELD_COUNT(catalog_generation_impulse_outputs), catalog_generation_impulse_config, FIELD_COUNT(catalog_generation_impulse_config)},
    {"generation_lfo", NULL, 0u, catalog_generation_lfo_outputs, FIELD_COUNT(catalog_generation_lfo_outputs), catalog_generation_lfo_config, FIELD_COUNT(catalog_generation_lfo_config)},
    {"generation_noise", NULL, 0u, catalog_generation_noise_outputs, FIELD_COUNT(catalog_generation_noise_outputs), catalog_generation_noise_config, FIELD_COUNT(catalog_generation_noise_config)},
    {"generation_oscillator", catalog_generation_oscillator_inputs, FIELD_COUNT(catalog_generation_oscillator_inputs), catalog_generation_oscillator_outputs, FIELD_COUNT(catalog_generation_oscillator_outputs), catalog_generation_oscillator_config, FIELD_COUNT(catalog_generation_oscillator_config)},
    {"interpolation_cubic", catalog_interpolation_cubic_inputs, FIELD_COUNT(catalog_interpolation_cubic_inputs), catalog_interpolation_cubic_outputs, FIELD_COUNT(catalog_interpolation_cubic_outputs), NULL, 0u},
    {"interpolation_lagrange", catalog_interpolation_lagrange_inputs, FIELD_COUNT(catalog_interpolation_lagrange_inputs), catalog_interpolation_lagrange_outputs, FIELD_COUNT(catalog_interpolation_lagrange_outputs), catalog_interpolation_lagrange_config, FIELD_COUNT(catalog_interpolation_lagrange_config)},
    {"interpolation_linear", catalog_interpolation_linear_inputs, FIELD_COUNT(catalog_interpolation_linear_inputs), catalog_interpolation_linear_outputs, FIELD_COUNT(catalog_interpolation_linear_outputs), NULL, 0u},
    {"interpolation_sinc", catalog_interpolation_sinc_inputs, FIELD_COUNT(catalog_interpolation_sinc_inputs), catalog_interpolation_sinc_outputs, FIELD_COUNT(catalog_interpolation_sinc_outputs), catalog_interpolation_sinc_config, FIELD_COUNT(catalog_interpolation_sinc_config)},
    {"mix_crossfade", catalog_mix_crossfade_inputs, FIELD_COUNT(catalog_mix_crossfade_inputs), catalog_mix_crossfade_outputs, FIELD_COUNT(catalog_mix_crossfade_outputs), catalog_mix_crossfade_config, FIELD_COUNT(catalog_mix_crossfade_config)},
    {"mix_decode_ms", catalog_mix_decode_ms_inputs, FIELD_COUNT(catalog_mix_decode_ms_inputs), catalog_mix_decode_ms_outputs, FIELD_COUNT(catalog_mix_decode_ms_outputs), NULL, 0u},
    {"mix_encode_ms", catalog_mix_encode_ms_inputs, FIELD_COUNT(catalog_mix_encode_ms_inputs), catalog_mix_encode_ms_outputs, FIELD_COUNT(catalog_mix_encode_ms_outputs), NULL, 0u},
    {"mix_matrix", catalog_mix_matrix_inputs, FIELD_COUNT(catalog_mix_matrix_inputs), catalog_mix_matrix_outputs, FIELD_COUNT(catalog_mix_matrix_outputs), catalog_mix_matrix_config, FIELD_COUNT(catalog_mix_matrix_config)},
    {"mix_pan_stereo", catalog_mix_pan_stereo_inputs, FIELD_COUNT(catalog_mix_pan_stereo_inputs), catalog_mix_pan_stereo_outputs, FIELD_COUNT(catalog_mix_pan_stereo_outputs), catalog_mix_pan_stereo_config, FIELD_COUNT(catalog_mix_pan_stereo_config)},
    {"mix_wet_dry", catalog_mix_wet_dry_inputs, FIELD_COUNT(catalog_mix_wet_dry_inputs), catalog_mix_wet_dry_outputs, FIELD_COUNT(catalog_mix_wet_dry_outputs), catalog_mix_wet_dry_config, FIELD_COUNT(catalog_mix_wet_dry_config)},
    {"modulation_amplitude", catalog_modulation_amplitude_inputs, FIELD_COUNT(catalog_modulation_amplitude_inputs), catalog_modulation_amplitude_outputs, FIELD_COUNT(catalog_modulation_amplitude_outputs), catalog_modulation_amplitude_config, FIELD_COUNT(catalog_modulation_amplitude_config)},
    {"modulation_frequency", catalog_modulation_frequency_inputs, FIELD_COUNT(catalog_modulation_frequency_inputs), catalog_modulation_frequency_outputs, FIELD_COUNT(catalog_modulation_frequency_outputs), catalog_modulation_frequency_config, FIELD_COUNT(catalog_modulation_frequency_config)},
    {"modulation_phase", catalog_modulation_phase_inputs, FIELD_COUNT(catalog_modulation_phase_inputs), catalog_modulation_phase_outputs, FIELD_COUNT(catalog_modulation_phase_outputs), catalog_modulation_phase_config, FIELD_COUNT(catalog_modulation_phase_config)},
    {"modulation_ring", catalog_modulation_ring_inputs, FIELD_COUNT(catalog_modulation_ring_inputs), catalog_modulation_ring_outputs, FIELD_COUNT(catalog_modulation_ring_outputs), NULL, 0u},
    {"modulation_scrub", catalog_modulation_scrub_inputs, FIELD_COUNT(catalog_modulation_scrub_inputs), catalog_modulation_scrub_outputs, FIELD_COUNT(catalog_modulation_scrub_outputs), catalog_modulation_scrub_config, FIELD_COUNT(catalog_modulation_scrub_config)},
    {"nonlinear_bitcrush", catalog_nonlinear_bitcrush_inputs, FIELD_COUNT(catalog_nonlinear_bitcrush_inputs), catalog_nonlinear_bitcrush_outputs, FIELD_COUNT(catalog_nonlinear_bitcrush_outputs), catalog_nonlinear_bitcrush_config, FIELD_COUNT(catalog_nonlinear_bitcrush_config)},
    {"nonlinear_sample_hold", catalog_nonlinear_sample_hold_inputs, FIELD_COUNT(catalog_nonlinear_sample_hold_inputs), catalog_nonlinear_sample_hold_outputs, FIELD_COUNT(catalog_nonlinear_sample_hold_outputs), catalog_nonlinear_sample_hold_config, FIELD_COUNT(catalog_nonlinear_sample_hold_config)},
    {"nonlinear_waveshape", catalog_nonlinear_waveshape_inputs, FIELD_COUNT(catalog_nonlinear_waveshape_inputs), catalog_nonlinear_waveshape_outputs, FIELD_COUNT(catalog_nonlinear_waveshape_outputs), catalog_nonlinear_waveshape_config, FIELD_COUNT(catalog_nonlinear_waveshape_config)},
    {"src_antialias", catalog_src_antialias_inputs, FIELD_COUNT(catalog_src_antialias_inputs), catalog_src_antialias_outputs, FIELD_COUNT(catalog_src_antialias_outputs), catalog_src_antialias_config, FIELD_COUNT(catalog_src_antialias_config)},
    {"src_antiimage", catalog_src_antiimage_inputs, FIELD_COUNT(catalog_src_antiimage_inputs), catalog_src_antiimage_outputs, FIELD_COUNT(catalog_src_antiimage_outputs), catalog_src_antiimage_config, FIELD_COUNT(catalog_src_antiimage_config)},
    {"src_convert_format", catalog_src_convert_format_inputs, FIELD_COUNT(catalog_src_convert_format_inputs), catalog_src_convert_format_outputs, FIELD_COUNT(catalog_src_convert_format_outputs), catalog_src_convert_format_config, FIELD_COUNT(catalog_src_convert_format_config)},
    {"src_downsample", catalog_src_downsample_inputs, FIELD_COUNT(catalog_src_downsample_inputs), catalog_src_downsample_outputs, FIELD_COUNT(catalog_src_downsample_outputs), catalog_src_downsample_config, FIELD_COUNT(catalog_src_downsample_config)},
    {"src_upsample", catalog_src_upsample_inputs, FIELD_COUNT(catalog_src_upsample_inputs), catalog_src_upsample_outputs, FIELD_COUNT(catalog_src_upsample_outputs), catalog_src_upsample_config, FIELD_COUNT(catalog_src_upsample_config)},
    {"freq_quantize", catalog_freq_quantize_inputs, FIELD_COUNT(catalog_freq_quantize_inputs), catalog_freq_quantize_outputs, FIELD_COUNT(catalog_freq_quantize_outputs), NULL, 0u},
};
