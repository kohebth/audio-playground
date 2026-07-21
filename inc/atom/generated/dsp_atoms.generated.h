/* Generated from schema/atoms/atoms.json by tools/generate_atom_artifacts.pl. Do not edit. */
#ifndef AUDIO_PLAYGROUND_DSP_ATOMS_GENERATED_H
#define AUDIO_PLAYGROUND_DSP_ATOMS_GENERATED_H

void amplitude_accumulate_process(
    amplitude_accumulate_out_t *out,
    const amplitude_accumulate_in_t *in,
    const amplitude_accumulate_params_t *params,
    amplitude_accumulate_state_t *state,
    const apg_process_context_t *context
);

void amplitude_latch_process(
    amplitude_latch_out_t *out,
    const amplitude_latch_in_t *in,
    const amplitude_latch_params_t *params,
    amplitude_latch_state_t *state,
    const apg_process_context_t *context
);

void amplitude_add_process(
    amplitude_add_out_t *out,
    const amplitude_add_in_t *in,
    const amplitude_add_params_t *params,
    amplitude_add_state_t *state,
    const apg_process_context_t *context
);

void amplitude_clip_hard_process(
    amplitude_clip_hard_out_t *out,
    const amplitude_clip_hard_in_t *in,
    const amplitude_clip_hard_params_t *params,
    amplitude_clip_hard_state_t *state,
    const apg_process_context_t *context
);

void amplitude_clip_soft_process(
    amplitude_clip_soft_out_t *out,
    const amplitude_clip_soft_in_t *in,
    const amplitude_clip_soft_params_t *params,
    amplitude_clip_soft_state_t *state,
    const apg_process_context_t *context
);

void amplitude_divide_process(
    amplitude_divide_out_t *out,
    const amplitude_divide_in_t *in,
    const amplitude_divide_params_t *params,
    amplitude_divide_state_t *state,
    const apg_process_context_t *context
);

void amplitude_gain_db_process(
    amplitude_gain_db_out_t *out,
    const amplitude_gain_db_in_t *in,
    const amplitude_gain_db_params_t *params,
    amplitude_gain_db_state_t *state,
    const apg_process_context_t *context
);

void amplitude_multiply_process(
    amplitude_multiply_out_t *out,
    const amplitude_multiply_in_t *in,
    const amplitude_multiply_params_t *params,
    amplitude_multiply_state_t *state,
    const apg_process_context_t *context
);

void amplitude_normalize_process(
    amplitude_normalize_out_t *out,
    const amplitude_normalize_in_t *in,
    const amplitude_normalize_params_t *params,
    amplitude_normalize_state_t *state,
    const apg_process_context_t *context
);

void amplitude_smooth_process(
    amplitude_smooth_out_t *out,
    const amplitude_smooth_in_t *in,
    const amplitude_smooth_params_t *params,
    amplitude_smooth_state_t *state,
    const apg_process_context_t *context
);

void amplitude_subtract_process(
    amplitude_subtract_out_t *out,
    const amplitude_subtract_in_t *in,
    const amplitude_subtract_params_t *params,
    amplitude_subtract_state_t *state,
    const apg_process_context_t *context
);

void delay_fractional_process(
    delay_fractional_out_t *out,
    const delay_fractional_in_t *in,
    const delay_fractional_params_t *params,
    delay_fractional_state_t *state,
    const apg_process_context_t *context
);

void delay_line_process(
    delay_line_out_t *out,
    const delay_line_in_t *in,
    const delay_line_params_t *params,
    delay_line_state_t *state,
    const apg_process_context_t *context
);

void delay_tap_feedback_process(
    delay_tap_feedback_out_t *out,
    const delay_tap_feedback_in_t *in,
    const delay_tap_feedback_params_t *params,
    delay_tap_feedback_state_t *state,
    const apg_process_context_t *context
);

void delay_tap_feedforward_process(
    delay_tap_feedforward_out_t *out,
    const delay_tap_feedforward_in_t *in,
    const delay_tap_feedforward_params_t *params,
    delay_tap_feedforward_state_t *state,
    const apg_process_context_t *context
);

void delay_unit_process(
    delay_unit_out_t *out,
    const delay_unit_in_t *in,
    const delay_unit_params_t *params,
    delay_unit_state_t *state,
    const apg_process_context_t *context
);

void detect_autocorrelate_process(
    detect_autocorrelate_out_t *out,
    const detect_autocorrelate_in_t *in,
    const detect_autocorrelate_params_t *params,
    detect_autocorrelate_state_t *state,
    const apg_process_context_t *context
);

void detect_pitch_process(
    detect_pitch_out_t *out,
    const detect_pitch_in_t *in,
    const detect_pitch_params_t *params,
    detect_pitch_state_t *state,
    const apg_process_context_t *context
);

void detect_envelope_process(
    detect_envelope_out_t *out,
    const detect_envelope_in_t *in,
    const detect_envelope_params_t *params,
    detect_envelope_state_t *state,
    const apg_process_context_t *context
);

void detect_peak_process(
    detect_peak_out_t *out,
    const detect_peak_in_t *in,
    const detect_peak_params_t *params,
    detect_peak_state_t *state,
    const apg_process_context_t *context
);

void detect_rms_process(
    detect_rms_out_t *out,
    const detect_rms_in_t *in,
    const detect_rms_params_t *params,
    detect_rms_state_t *state,
    const apg_process_context_t *context
);

void detect_slope_process(
    detect_slope_out_t *out,
    const detect_slope_in_t *in,
    const detect_slope_params_t *params,
    detect_slope_state_t *state,
    const apg_process_context_t *context
);

void detect_threshold_process(
    detect_threshold_out_t *out,
    const detect_threshold_in_t *in,
    const detect_threshold_params_t *params,
    detect_threshold_state_t *state,
    const apg_process_context_t *context
);

void detect_zero_crossing_process(
    detect_zero_crossing_out_t *out,
    const detect_zero_crossing_in_t *in,
    const detect_zero_crossing_params_t *params,
    detect_zero_crossing_state_t *state,
    const apg_process_context_t *context
);

void filter_allpass_process(
    filter_allpass_out_t *out,
    const filter_allpass_in_t *in,
    const filter_allpass_params_t *params,
    filter_allpass_state_t *state,
    const apg_process_context_t *context
);

void filter_biquad_coefficients_process(
    filter_biquad_coefficients_out_t *out,
    const filter_biquad_coefficients_in_t *in,
    const filter_biquad_coefficients_params_t *params,
    filter_biquad_coefficients_state_t *state,
    const apg_process_context_t *context
);

void filter_biquad_process(
    filter_biquad_out_t *out,
    const filter_biquad_in_t *in,
    const filter_biquad_params_t *params,
    filter_biquad_state_t *state,
    const apg_process_context_t *context
);

void filter_comb_fb_process(
    filter_comb_fb_out_t *out,
    const filter_comb_fb_in_t *in,
    const filter_comb_fb_params_t *params,
    filter_comb_fb_state_t *state,
    const apg_process_context_t *context
);

void filter_comb_ff_process(
    filter_comb_ff_out_t *out,
    const filter_comb_ff_in_t *in,
    const filter_comb_ff_params_t *params,
    filter_comb_ff_state_t *state,
    const apg_process_context_t *context
);

void filter_dc_block_process(
    filter_dc_block_out_t *out,
    const filter_dc_block_in_t *in,
    const filter_dc_block_params_t *params,
    filter_dc_block_state_t *state,
    const apg_process_context_t *context
);

void filter_differentiate_process(
    filter_differentiate_out_t *out,
    const filter_differentiate_in_t *in,
    const filter_differentiate_params_t *params,
    filter_differentiate_state_t *state,
    const apg_process_context_t *context
);

void filter_fir_process(
    filter_fir_out_t *out,
    const filter_fir_in_t *in,
    const filter_fir_params_t *params,
    filter_fir_state_t *state,
    const apg_process_context_t *context
);

void filter_integrate_process(
    filter_integrate_out_t *out,
    const filter_integrate_in_t *in,
    const filter_integrate_params_t *params,
    filter_integrate_state_t *state,
    const apg_process_context_t *context
);

void freq_fft_process(
    freq_fft_out_t *out,
    const freq_fft_in_t *in,
    const freq_fft_params_t *params,
    freq_fft_state_t *state,
    const apg_spectral_info_t *context
);

void freq_ifft_process(
    freq_ifft_out_t *out,
    const freq_ifft_in_t *in,
    const freq_ifft_params_t *params,
    freq_ifft_state_t *state,
    const apg_spectral_info_t *context
);

void freq_multiply_process(
    freq_multiply_out_t *out,
    const freq_multiply_in_t *in,
    const freq_multiply_params_t *params,
    freq_multiply_state_t *state,
    const apg_spectral_info_t *context
);

void freq_overlap_add_process(
    freq_overlap_add_out_t *out,
    const freq_overlap_add_in_t *in,
    const freq_overlap_add_params_t *params,
    freq_overlap_add_state_t *state,
    const apg_process_context_t *context
);

void freq_overlap_add_spectral_process(
    freq_overlap_add_out_t *out,
    const freq_overlap_add_in_t *in,
    const freq_overlap_add_params_t *params,
    freq_overlap_add_state_t *state,
    const apg_spectral_info_t *context
);

void freq_overlap_save_process(
    freq_overlap_save_out_t *out,
    const freq_overlap_save_in_t *in,
    const freq_overlap_save_params_t *params,
    freq_overlap_save_state_t *state,
    const apg_process_context_t *context
);

void freq_overlap_save_spectral_process(
    freq_overlap_save_out_t *out,
    const freq_overlap_save_in_t *in,
    const freq_overlap_save_params_t *params,
    freq_overlap_save_state_t *state,
    const apg_spectral_info_t *context
);

void freq_window_process(
    freq_window_out_t *out,
    const freq_window_in_t *in,
    const freq_window_params_t *params,
    freq_window_state_t *state,
    const apg_process_context_t *context
);

void freq_window_spectral_process(
    freq_window_out_t *out,
    const freq_window_in_t *in,
    const freq_window_params_t *params,
    freq_window_state_t *state,
    const apg_spectral_info_t *context
);

void freq_shift_process(
    freq_shift_out_t *out,
    const freq_shift_in_t *in,
    const freq_shift_params_t *params,
    freq_shift_state_t *state,
    const apg_process_context_t *context
);

void generation_dc_process(
    generation_dc_out_t *out,
    const generation_dc_in_t *in,
    const generation_dc_params_t *params,
    generation_dc_state_t *state,
    const apg_process_context_t *context
);

void generation_envelope_process(
    generation_envelope_out_t *out,
    const generation_envelope_in_t *in,
    const generation_envelope_params_t *params,
    generation_envelope_state_t *state,
    const apg_process_context_t *context
);

void generation_impulse_process(
    generation_impulse_out_t *out,
    const generation_impulse_in_t *in,
    const generation_impulse_params_t *params,
    generation_impulse_state_t *state,
    const apg_process_context_t *context
);

void generation_lfo_process(
    generation_lfo_out_t *out,
    const generation_lfo_in_t *in,
    const generation_lfo_params_t *params,
    generation_lfo_state_t *state,
    const apg_process_context_t *context
);

void generation_noise_process(
    generation_noise_out_t *out,
    const generation_noise_in_t *in,
    const generation_noise_params_t *params,
    generation_noise_state_t *state,
    const apg_process_context_t *context
);

void generation_oscillator_process(
    generation_oscillator_out_t *out,
    const generation_oscillator_in_t *in,
    const generation_oscillator_params_t *params,
    generation_oscillator_state_t *state,
    const apg_process_context_t *context
);

void interpolation_cubic_process(
    interpolation_cubic_out_t *out,
    const interpolation_cubic_in_t *in,
    const interpolation_cubic_params_t *params,
    interpolation_cubic_state_t *state,
    const apg_process_context_t *context
);

void interpolation_lagrange_process(
    interpolation_lagrange_out_t *out,
    const interpolation_lagrange_in_t *in,
    const interpolation_lagrange_params_t *params,
    interpolation_lagrange_state_t *state,
    const apg_process_context_t *context
);

void interpolation_linear_process(
    interpolation_linear_out_t *out,
    const interpolation_linear_in_t *in,
    const interpolation_linear_params_t *params,
    interpolation_linear_state_t *state,
    const apg_process_context_t *context
);

void interpolation_sinc_process(
    interpolation_sinc_out_t *out,
    const interpolation_sinc_in_t *in,
    const interpolation_sinc_params_t *params,
    interpolation_sinc_state_t *state,
    const apg_process_context_t *context
);

void mix_crossfade_process(
    mix_crossfade_out_t *out,
    const mix_crossfade_in_t *in,
    const mix_crossfade_params_t *params,
    mix_crossfade_state_t *state,
    const apg_process_context_t *context
);

void mix_decode_ms_process(
    mix_decode_ms_out_t *out,
    const mix_decode_ms_in_t *in,
    const mix_decode_ms_params_t *params,
    mix_decode_ms_state_t *state,
    const apg_process_context_t *context
);

void mix_encode_ms_process(
    mix_encode_ms_out_t *out,
    const mix_encode_ms_in_t *in,
    const mix_encode_ms_params_t *params,
    mix_encode_ms_state_t *state,
    const apg_process_context_t *context
);

void mix_matrix_process(
    mix_matrix_out_t *out,
    const mix_matrix_in_t *in,
    const mix_matrix_params_t *params,
    mix_matrix_state_t *state,
    const apg_process_context_t *context
);

void mix_pan_stereo_process(
    mix_pan_stereo_out_t *out,
    const mix_pan_stereo_in_t *in,
    const mix_pan_stereo_params_t *params,
    mix_pan_stereo_state_t *state,
    const apg_process_context_t *context
);

void mix_wet_dry_process(
    mix_wet_dry_out_t *out,
    const mix_wet_dry_in_t *in,
    const mix_wet_dry_params_t *params,
    mix_wet_dry_state_t *state,
    const apg_process_context_t *context
);

void modulation_amplitude_process(
    modulation_amplitude_out_t *out,
    const modulation_amplitude_in_t *in,
    const modulation_amplitude_params_t *params,
    modulation_amplitude_state_t *state,
    const apg_process_context_t *context
);

void modulation_frequency_process(
    modulation_frequency_out_t *out,
    const modulation_frequency_in_t *in,
    const modulation_frequency_params_t *params,
    modulation_frequency_state_t *state,
    const apg_process_context_t *context
);

void modulation_phase_process(
    modulation_phase_out_t *out,
    const modulation_phase_in_t *in,
    const modulation_phase_params_t *params,
    modulation_phase_state_t *state,
    const apg_process_context_t *context
);

void modulation_phaser_process(
    modulation_phaser_out_t *out,
    const modulation_phaser_in_t *in,
    const modulation_phaser_params_t *params,
    modulation_phaser_state_t *state,
    const apg_process_context_t *context
);

void modulation_ring_process(
    modulation_ring_out_t *out,
    const modulation_ring_in_t *in,
    const modulation_ring_params_t *params,
    modulation_ring_state_t *state,
    const apg_process_context_t *context
);

void modulation_scrub_process(
    modulation_scrub_out_t *out,
    const modulation_scrub_in_t *in,
    const modulation_scrub_params_t *params,
    modulation_scrub_state_t *state,
    const apg_process_context_t *context
);

void nonlinear_bitcrush_process(
    nonlinear_bitcrush_out_t *out,
    const nonlinear_bitcrush_in_t *in,
    const nonlinear_bitcrush_params_t *params,
    nonlinear_bitcrush_state_t *state,
    const apg_process_context_t *context
);

void nonlinear_sample_hold_process(
    nonlinear_sample_hold_out_t *out,
    const nonlinear_sample_hold_in_t *in,
    const nonlinear_sample_hold_params_t *params,
    nonlinear_sample_hold_state_t *state,
    const apg_process_context_t *context
);

void nonlinear_waveshape_process(
    nonlinear_waveshape_out_t *out,
    const nonlinear_waveshape_in_t *in,
    const nonlinear_waveshape_params_t *params,
    nonlinear_waveshape_state_t *state,
    const apg_process_context_t *context
);

void src_antialias_process(
    src_antialias_out_t *out,
    const src_antialias_in_t *in,
    const src_antialias_params_t *params,
    src_antialias_state_t *state,
    const apg_process_context_t *context
);

void src_antiimage_process(
    src_antiimage_out_t *out,
    const src_antiimage_in_t *in,
    const src_antiimage_params_t *params,
    src_antiimage_state_t *state,
    const apg_process_context_t *context
);

void src_convert_format_process(
    src_convert_format_out_t *out,
    const src_convert_format_in_t *in,
    const src_convert_format_params_t *params,
    src_convert_format_state_t *state,
    const apg_process_context_t *context
);

apg_stream_result_t src_downsample_process(
    src_downsample_out_t *out,
    const src_downsample_in_t *in,
    const src_downsample_params_t *params,
    src_downsample_state_t *state,
    const apg_stream_context_t *context
);

apg_stream_result_t src_upsample_process(
    src_upsample_out_t *out,
    const src_upsample_in_t *in,
    const src_upsample_params_t *params,
    src_upsample_state_t *state,
    const apg_stream_context_t *context
);

void freq_quantize_process(
    freq_quantize_out_t *out,
    const freq_quantize_in_t *in,
    const freq_quantize_params_t *params,
    freq_quantize_state_t *state,
    const apg_process_context_t *context
);

void math_difference_process(
    math_difference_out_t *out,
    const math_difference_in_t *in,
    const math_difference_params_t *params,
    math_difference_state_t *state,
    const apg_process_context_t *context
);

void math_integrate_process(
    math_integrate_out_t *out,
    const math_integrate_in_t *in,
    const math_integrate_params_t *params,
    math_integrate_state_t *state,
    const apg_process_context_t *context
);

#endif // AUDIO_PLAYGROUND_DSP_ATOMS_GENERATED_H
