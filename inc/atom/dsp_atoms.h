#ifndef DSP_ATOMS_H
#define DSP_ATOMS_H

#include <stdint.h>
#include <stddef.h>
#include <math.h>

typedef float *Signal;
typedef float *Spectrum;
typedef float *Buffer;

// Standard IO Structs for Atom API
typedef struct { float *signal; } atom_mono_t;
typedef struct { float *signal_a; float *signal_b; } atom_pair_t;
typedef struct { float *left; float *right; } atom_stereo_t;
typedef struct { float *real; float *imag; } atom_complex_t;
typedef struct { float *mid; float *side; } atom_ms_t;
typedef struct { float *dry; float *wet; } atom_wet_dry_t;
typedef struct { float *numerator; float *denominator; } atom_div_t;

typedef enum {
    WAVEFORM_SINE,
    WAVEFORM_SAW,
    WAVEFORM_SQUARE,
    WAVEFORM_TRIANGLE,
    WAVEFORM_NOISE_WHITE,
    WAVEFORM_NOISE_PINK,
    WAVEFORM_NOISE_BROWN,
} WaveformType;

typedef enum {
    NORMALIZE_PEAK,
    NORMALIZE_RMS,
} NormalizeMode;

typedef enum {
    INTERPOLATION_LINEAR,
    INTERPOLATION_CUBIC,
    INTERPOLATION_SINC,
    INTERPOLATION_LAGRANGE,
} InterpolationType;

typedef enum {
    WINDOW_HANN,
    WINDOW_HAMMING,
    WINDOW_BLACKMAN,
    WINDOW_RECTANGULAR,
} WindowType;

/* 🔷 Signal Generation */

typedef struct { float *signal; } generation_oscillator_out_t;
typedef struct { float frequency; int waveform; float phase_offset; float sample_rate; } generation_oscillator_params_t;
typedef struct { float phase; } generation_oscillator_state_t;
void generation_oscillator(generation_oscillator_out_t out, void *in, generation_oscillator_params_t params, generation_oscillator_state_t *state);

typedef struct { float *signal; } generation_noise_out_t;
typedef struct { float amplitude; int color; } generation_noise_params_t;
typedef struct { uint32_t seed; float prev_value; } generation_noise_state_t;
void generation_noise(generation_noise_out_t out, void *in, generation_noise_params_t params, generation_noise_state_t *state);

typedef struct { float *signal; } generation_envelope_out_t;
typedef struct { float *gate; } generation_envelope_in_t;
typedef struct { float attack; float decay; float sustain; float release; float sample_rate; } generation_envelope_params_t;
typedef struct { float current_level; int stage; } generation_envelope_state_t;
void generation_envelope(generation_envelope_out_t out, generation_envelope_in_t in, generation_envelope_params_t params, generation_envelope_state_t *state);

typedef struct { float *signal; } generation_lfo_out_t;
typedef struct { float frequency; int waveform; float phase_offset; float sample_rate; } generation_lfo_params_t;
typedef struct { float phase; } generation_lfo_state_t;
void generation_lfo(generation_lfo_out_t out, void *in, generation_lfo_params_t params, generation_lfo_state_t *state);

typedef struct { float *signal; } generation_impulse_out_t;
typedef struct { float interval; float sample_rate; } generation_impulse_params_t;
typedef struct { int counter; } generation_impulse_state_t;
void generation_impulse(generation_impulse_out_t out, void *in, generation_impulse_params_t params, generation_impulse_state_t *state);

typedef struct { float *signal; } generation_dc_out_t;
typedef struct { float value; } generation_dc_params_t;
void generation_dc(generation_dc_out_t out, void *in, generation_dc_params_t params, void *state);

/* 🔷 Gain & Amplitude */

typedef struct { float *signal; } amplitude_multiply_out_t;
typedef struct { float *signal_a; float *signal_b; } amplitude_multiply_in_t;
void amplitude_multiply(amplitude_multiply_out_t out, amplitude_multiply_in_t in, void *params, void *state);

typedef struct { float *signal; } amplitude_divide_out_t;
typedef struct { float *numerator; float *denominator; } amplitude_divide_in_t;
typedef struct { float epsilon; } amplitude_divide_params_t;
void amplitude_divide(amplitude_divide_out_t out, amplitude_divide_in_t in, amplitude_divide_params_t params, void *state);

typedef struct { float *signal; } amplitude_smooth_out_t;
typedef struct { float *signal; } amplitude_smooth_in_t;
typedef struct { float attack; float release; float sample_rate; } amplitude_smooth_params_t;
typedef struct { float prev_value; } amplitude_smooth_state_t;
void amplitude_smooth(amplitude_smooth_out_t out, amplitude_smooth_in_t in, amplitude_smooth_params_t params, amplitude_smooth_state_t *state);

typedef struct { float *signal; } amplitude_clip_hard_out_t;
typedef struct { float *signal; } amplitude_clip_hard_in_t;
typedef struct { float threshold; } amplitude_clip_hard_params_t;
void amplitude_clip_hard(amplitude_clip_hard_out_t out, amplitude_clip_hard_in_t in, amplitude_clip_hard_params_t params, void *state);

typedef struct { float *signal; } amplitude_clip_soft_out_t;
typedef struct { float *signal; } amplitude_clip_soft_in_t;
typedef struct { float threshold; int curve; } amplitude_clip_soft_params_t;
void amplitude_clip_soft(amplitude_clip_soft_out_t out, amplitude_clip_soft_in_t in, amplitude_clip_soft_params_t params, void *state);


typedef struct { float *signal; } amplitude_normalize_out_t;
typedef struct { float *signal; } amplitude_normalize_in_t;
typedef struct { float target_level; int mode; } amplitude_normalize_params_t;
typedef struct { float running_peak; } amplitude_normalize_state_t;
void amplitude_normalize(amplitude_normalize_out_t out, amplitude_normalize_in_t in, amplitude_normalize_params_t params, amplitude_normalize_state_t *state);

typedef struct { float *signal; } amplitude_add_out_t;
typedef struct { float *signal_a; float *signal_b; } amplitude_add_in_t;
void amplitude_add(amplitude_add_out_t out, amplitude_add_in_t in, void *params, void *state);

typedef struct { float *signal; } amplitude_subtract_out_t;
typedef struct { float *signal_a; float *signal_b; } amplitude_subtract_in_t;
void amplitude_subtract(amplitude_subtract_out_t out, amplitude_subtract_in_t in, void *params, void *state);

typedef struct { float *signal; } amplitude_accumulate_out_t;
typedef struct { float *signal; } amplitude_accumulate_in_t;
typedef struct { float accumulator; } amplitude_accumulate_state_t;
void amplitude_accumulate(amplitude_accumulate_out_t out, amplitude_accumulate_in_t in, void *params, amplitude_accumulate_state_t *state);


/* 🔷 Delay & Memory */

typedef struct { float *signal; } delay_unit_out_t;
typedef struct { float *signal; } delay_unit_in_t;
typedef struct { float prev_sample; } delay_unit_state_t;
void delay_unit(delay_unit_out_t out, delay_unit_in_t in, void *params, delay_unit_state_t *state);

typedef struct { float *signal; } delay_line_out_t;
typedef struct { float *signal; } delay_line_in_t;
typedef struct { int length; float sample_rate; } delay_line_params_t;
typedef struct { float *buffer; int write_pos; } delay_line_state_t;
void delay_line(delay_line_out_t out, delay_line_in_t in, delay_line_params_t params, delay_line_state_t *state);

typedef struct { float *signal; } delay_fractional_out_t;
typedef struct { float *signal; } delay_fractional_in_t;
typedef struct { float delay_samples; int interpolation; } delay_fractional_params_t;
typedef struct { float *buffer; int write_pos; } delay_fractional_state_t;
void delay_fractional(delay_fractional_out_t out, delay_fractional_in_t in, delay_fractional_params_t params, delay_fractional_state_t *state);

typedef struct { float *signal; } delay_tap_feedback_out_t;
typedef struct { float *buffer; int tap_position; } delay_tap_feedback_in_t;
typedef struct { float coefficient; } delay_tap_feedback_params_t;
void delay_tap_feedback(delay_tap_feedback_out_t out, delay_tap_feedback_in_t in, delay_tap_feedback_params_t params, void *state);

typedef struct { float *signal; } delay_tap_feedforward_out_t;
typedef struct { float *buffer; int tap_position; } delay_tap_feedforward_in_t;
typedef struct { float coefficient; } delay_tap_feedforward_params_t;
void delay_tap_feedforward(delay_tap_feedforward_out_t out, delay_tap_feedforward_in_t in, delay_tap_feedforward_params_t params, void *state);


/* 🔷 Filtering */

typedef struct { float *signal; } filter_fir_out_t;
typedef struct { float *signal; } filter_fir_in_t;
typedef struct { float *kernel; int kernel_size; } filter_fir_params_t;
typedef struct { float *buffer; int write_pos; } filter_fir_state_t;
void filter_fir(filter_fir_out_t out, filter_fir_in_t in, filter_fir_params_t params, filter_fir_state_t *state);

typedef struct { float *signal; } filter_biquad_out_t;
typedef struct { float *signal; } filter_biquad_in_t;
typedef struct { float b0; float b1; float b2; float a1; float a2; } filter_biquad_params_t;
typedef struct { float z1; float z2; } filter_biquad_state_t;
void filter_biquad(filter_biquad_out_t out, filter_biquad_in_t in, filter_biquad_params_t params, filter_biquad_state_t *state);

typedef struct { float *signal; } filter_dc_block_out_t;
typedef struct { float *signal; } filter_dc_block_in_t;
typedef struct { float coefficient; } filter_dc_block_params_t;
typedef struct { float prev_input; float prev_output; } filter_dc_block_state_t;
void filter_dc_block(filter_dc_block_out_t out, filter_dc_block_in_t in, filter_dc_block_params_t params, filter_dc_block_state_t *state);

typedef struct { float *signal; } filter_comb_ff_out_t;
typedef struct { float *signal; } filter_comb_ff_in_t;
typedef struct { int delay_samples; float coefficient; } filter_comb_ff_params_t;
typedef struct { float *buffer; int write_pos; } filter_comb_ff_state_t;
void filter_comb_ff(filter_comb_ff_out_t out, filter_comb_ff_in_t in, filter_comb_ff_params_t params, filter_comb_ff_state_t *state);

typedef struct { float *signal; } filter_comb_fb_out_t;
typedef struct { float *signal; } filter_comb_fb_in_t;
typedef struct { int delay_samples; float coefficient; } filter_comb_fb_params_t;
typedef struct { float *buffer; int write_pos; } filter_comb_fb_state_t;
void filter_comb_fb(filter_comb_fb_out_t out, filter_comb_fb_in_t in, filter_comb_fb_params_t params, filter_comb_fb_state_t *state);

typedef struct { float *signal; } filter_allpass_out_t;
typedef struct { float *signal; } filter_allpass_in_t;
typedef struct { int delay_samples; float coefficient; } filter_allpass_params_t;
typedef struct { float *buffer; int write_pos; } filter_allpass_state_t;
void filter_allpass(filter_allpass_out_t out, filter_allpass_in_t in, filter_allpass_params_t params, filter_allpass_state_t *state);

typedef struct { float *signal; } filter_integrate_out_t;
typedef struct { float *signal; } filter_integrate_in_t;
typedef struct { float accumulator; } filter_integrate_state_t;
void filter_integrate(filter_integrate_out_t out, filter_integrate_in_t in, void *params, filter_integrate_state_t *state);

typedef struct { float *signal; } filter_differentiate_out_t;
typedef struct { float *signal; } filter_differentiate_in_t;
typedef struct { float prev_sample; } filter_differentiate_state_t;
void filter_differentiate(filter_differentiate_out_t out, filter_differentiate_in_t in, void *params, filter_differentiate_state_t *state);


/* 🔷 Detection & Analysis */

typedef struct { float *level; } detect_peak_out_t;
typedef struct { float *signal; } detect_peak_in_t;
typedef struct { float attack; float release; float sample_rate; } detect_peak_params_t;
typedef struct { float prev_peak; } detect_peak_state_t;
void detect_peak(detect_peak_out_t out, detect_peak_in_t in, detect_peak_params_t params, detect_peak_state_t *state);

typedef struct { float *envelope; } detect_envelope_out_t;
typedef struct { float *signal; } detect_envelope_in_t;
typedef struct { float attack; float release; float sample_rate; } detect_envelope_params_t;
typedef struct { float prev_envelope; } detect_envelope_state_t;
void detect_envelope(detect_envelope_out_t out, detect_envelope_in_t in, detect_envelope_params_t params, detect_envelope_state_t *state);

typedef struct { float *gate; } detect_threshold_out_t;
typedef struct { float *signal; } detect_threshold_in_t;
typedef struct { float threshold; } detect_threshold_params_t;
void detect_threshold(detect_threshold_out_t out, detect_threshold_in_t in, detect_threshold_params_t params, void *state);

typedef struct { float *level; } detect_rms_out_t;
typedef struct { float *signal; } detect_rms_in_t;
typedef struct { int window_size; } detect_rms_params_t;
typedef struct { float *buffer; int write_pos; float sum; } detect_rms_state_t;
void detect_rms(detect_rms_out_t out, detect_rms_in_t in, detect_rms_params_t params, detect_rms_state_t *state);

typedef struct { float *trigger; } detect_zero_crossing_out_t;
typedef struct { float *signal; } detect_zero_crossing_in_t;
typedef struct { float prev_sample; } detect_zero_crossing_state_t;
void detect_zero_crossing(detect_zero_crossing_out_t out, detect_zero_crossing_in_t in, void *params, detect_zero_crossing_state_t *state);

typedef struct { float *slope; } detect_slope_out_t;
typedef struct { float *signal; } detect_slope_in_t;
typedef struct { float prev_sample; } detect_slope_state_t;
void detect_slope(detect_slope_out_t out, detect_slope_in_t in, void *params, detect_slope_state_t *state);

typedef struct { float *correlation; } detect_autocorrelate_out_t;
typedef struct { float *signal; } detect_autocorrelate_in_t;
typedef struct { int max_lag; } detect_autocorrelate_params_t;
typedef struct { float *buffer; int write_pos; } detect_autocorrelate_state_t;
void detect_autocorrelate(detect_autocorrelate_out_t out, detect_autocorrelate_in_t in, detect_autocorrelate_params_t params, detect_autocorrelate_state_t *state);

/* 🔷 Modulation */

typedef struct { float *signal; } modulation_phase_out_t;
typedef struct { float *signal; float *modulator; } modulation_phase_in_t;
typedef struct { float depth; } modulation_phase_params_t;
typedef struct { float *buffer; int write_pos; } modulation_phase_state_t;
void modulation_phase(modulation_phase_out_t out, modulation_phase_in_t in, modulation_phase_params_t params, modulation_phase_state_t *state);

typedef struct { float *signal; } modulation_ring_out_t;
typedef struct { float *signal; float *modulator; } modulation_ring_in_t;
void modulation_ring(modulation_ring_out_t out, modulation_ring_in_t in, void *params, void *state);

typedef struct { float *signal; } modulation_amplitude_out_t;
typedef struct { float *signal; float *modulator; } modulation_amplitude_in_t;
typedef struct { float depth; } modulation_amplitude_params_t;
void modulation_amplitude(modulation_amplitude_out_t out, modulation_amplitude_in_t in, modulation_amplitude_params_t params, void *state);

typedef struct { float *signal; } modulation_frequency_out_t;
typedef struct { float *signal; float *modulator; } modulation_frequency_in_t;
typedef struct { float depth; float sample_rate; } modulation_frequency_params_t;
typedef struct { float *buffer; int write_pos; float current_delay; } modulation_frequency_state_t;
void modulation_frequency(modulation_frequency_out_t out, modulation_frequency_in_t in, modulation_frequency_params_t params, modulation_frequency_state_t *state);

typedef struct { float *signal; } modulation_scrub_out_t;
typedef struct { float *buffer; float *position; } modulation_scrub_in_t;
typedef struct { int buffer_size; } modulation_scrub_params_t;
void modulation_scrub(modulation_scrub_out_t out, modulation_scrub_in_t in, modulation_scrub_params_t params, void *state);

/* 🔷 Interpolation */

typedef struct { float *signal; } interpolation_linear_out_t;
typedef struct { float *signal_a; float *signal_b; float *t; } interpolation_linear_in_t;
void interpolation_linear(interpolation_linear_out_t out, interpolation_linear_in_t in, void *params, void *state);

typedef struct { float *signal; } interpolation_cubic_out_t;
typedef struct { float *signal_n1; float *signal_a; float *signal_b; float *signal_c; float *t; } interpolation_cubic_in_t;
void interpolation_cubic(interpolation_cubic_out_t out, interpolation_cubic_in_t in, void *params, void *state);

typedef struct { float *signal; } interpolation_sinc_out_t;
typedef struct { float *buffer; float *position; } interpolation_sinc_in_t;
typedef struct { int num_taps; } interpolation_sinc_params_t;
void interpolation_sinc(interpolation_sinc_out_t out, interpolation_sinc_in_t in, interpolation_sinc_params_t params, void *state);

typedef struct { float *signal; } interpolation_lagrange_out_t;
typedef struct { float *samples; float *t; } interpolation_lagrange_in_t;
typedef struct { int order; } interpolation_lagrange_params_t;
void interpolation_lagrange(interpolation_lagrange_out_t out, interpolation_lagrange_in_t in, interpolation_lagrange_params_t params, void *state);


/* 🔷 Sample Rate Conversion */

typedef struct { float *signal; } src_upsample_out_t;
typedef struct { float *signal; } src_upsample_in_t;
typedef struct { int factor; } src_upsample_params_t;
void src_upsample(src_upsample_out_t out, src_upsample_in_t in, src_upsample_params_t params, void *state);

typedef struct { float *signal; } src_downsample_out_t;
typedef struct { float *signal; } src_downsample_in_t;
typedef struct { int factor; } src_downsample_params_t;
void src_downsample(src_downsample_out_t out, src_downsample_in_t in, src_downsample_params_t params, void *state);

typedef struct { float *signal; } src_antialias_out_t;
typedef struct { float *signal; } src_antialias_in_t;
typedef struct { float cutoff; float sample_rate; } src_antialias_params_t;
typedef struct { float z1; float z2; } src_antialias_state_t;
void src_antialias(src_antialias_out_t out, src_antialias_in_t in, src_antialias_params_t params, src_antialias_state_t *state);

typedef struct { float *signal; } src_antiimage_out_t;
typedef struct { float *signal; } src_antiimage_in_t;
typedef struct { float cutoff; float sample_rate; } src_antiimage_params_t;
typedef struct { float z1; float z2; } src_antiimage_state_t;
void src_antiimage(src_antiimage_out_t out, src_antiimage_in_t in, src_antiimage_params_t params, src_antiimage_state_t *state);

typedef struct { float *signal; } src_convert_format_out_t;
typedef struct { float *signal; } src_convert_format_in_t;
typedef struct { int from_format; int to_format; } src_convert_format_params_t;
void src_convert_format(src_convert_format_out_t out, src_convert_format_in_t in, src_convert_format_params_t params, void *state);


/* 🔷 Frequency Domain */

typedef struct { float *real; float *imag; } freq_fft_out_t;
typedef struct { float *signal; } freq_fft_in_t;
typedef struct { int block_size; } freq_fft_params_t;
void freq_fft(freq_fft_out_t out, freq_fft_in_t in, freq_fft_params_t params, void *state);

typedef struct { float *signal; } freq_ifft_out_t;
typedef struct { float *real; float *imag; } freq_ifft_in_t;
typedef struct { int block_size; } freq_ifft_params_t;
void freq_ifft(freq_ifft_out_t out, freq_ifft_in_t in, freq_ifft_params_t params, void *state);

typedef struct { float *signal; } freq_window_out_t;
typedef struct { float *signal; } freq_window_in_t;
typedef struct { int window_type; int block_size; } freq_window_params_t;
void freq_window(freq_window_out_t out, freq_window_in_t in, freq_window_params_t params, void *state);

typedef struct { float *real; float *imag; } freq_multiply_out_t;
typedef struct { float *real_a; float *imag_a; float *real_b; float *imag_b; } freq_multiply_in_t;
typedef struct { int block_size; } freq_multiply_params_t;
void freq_multiply(freq_multiply_out_t out, freq_multiply_in_t in, freq_multiply_params_t params, void *state);

typedef struct { float *signal; } freq_overlap_add_out_t;
typedef struct { float *frame; } freq_overlap_add_in_t;
typedef struct { int block_size; int hop_size; } freq_overlap_add_params_t;
typedef struct { float *buffer; } freq_overlap_add_state_t;
void freq_overlap_add(freq_overlap_add_out_t out, freq_overlap_add_in_t in, freq_overlap_add_params_t params, freq_overlap_add_state_t *state);

typedef struct { float *frame; } freq_overlap_save_out_t;
typedef struct { float *signal; } freq_overlap_save_in_t;
typedef struct { int block_size; int hop_size; } freq_overlap_save_params_t;
typedef struct { float *buffer; int write_pos; } freq_overlap_save_state_t;
void freq_overlap_save(freq_overlap_save_out_t out, freq_overlap_save_in_t in, freq_overlap_save_params_t params, freq_overlap_save_state_t *state);

/* 🔷 Mixing & Routing */

typedef struct { float *signal; } mix_crossfade_out_t;
typedef struct { float *signal_a; float *signal_b; } mix_crossfade_in_t;
typedef struct { float t; } mix_crossfade_params_t;
void mix_crossfade(mix_crossfade_out_t out, mix_crossfade_in_t in, mix_crossfade_params_t params, void *state);

typedef struct { float *signal; } mix_wet_dry_out_t;
typedef struct { float *dry; float *wet; } mix_wet_dry_in_t;
typedef struct { float mix; } mix_wet_dry_params_t;
void mix_wet_dry(mix_wet_dry_out_t out, mix_wet_dry_in_t in, mix_wet_dry_params_t params, void *state);

typedef struct { float **signals; } mix_matrix_out_t;
typedef struct { float **signals; } mix_matrix_in_t;
typedef struct { float **coefficients; int num_in; int num_out; } mix_matrix_params_t;
void mix_matrix(mix_matrix_out_t out, mix_matrix_in_t in, mix_matrix_params_t params, void *state);

typedef struct { float *left; float *right; } mix_pan_stereo_out_t;
typedef struct { float *signal; } mix_pan_stereo_in_t;
typedef struct { float position; } mix_pan_stereo_params_t;
void mix_pan_stereo(mix_pan_stereo_out_t out, mix_pan_stereo_in_t in, mix_pan_stereo_params_t params, void *state);

typedef struct { float *mid; float *side; } mix_encode_ms_out_t;
typedef struct { float *left; float *right; } mix_encode_ms_in_t;
void mix_encode_ms(mix_encode_ms_out_t out, mix_encode_ms_in_t in, void *params, void *state);

typedef struct { float *left; float *right; } mix_decode_ms_out_t;
typedef struct { float *mid; float *side; } mix_decode_ms_in_t;
void mix_decode_ms(mix_decode_ms_out_t out, mix_decode_ms_in_t in, void *params, void *state);

/* 🔷 Nonlinear / Distortion */

typedef struct { float *signal; } nonlinear_waveshape_out_t;
typedef struct { float *signal; } nonlinear_waveshape_in_t;
typedef struct { float *transfer_table; int table_size; } nonlinear_waveshape_params_t;
void nonlinear_waveshape(nonlinear_waveshape_out_t out, nonlinear_waveshape_in_t in, nonlinear_waveshape_params_t params, void *state);

typedef struct { float *signal; } nonlinear_bitcrush_out_t;
typedef struct { float *signal; } nonlinear_bitcrush_in_t;
typedef struct { float bit_depth; } nonlinear_bitcrush_params_t;
void nonlinear_bitcrush(nonlinear_bitcrush_out_t out, nonlinear_bitcrush_in_t in, nonlinear_bitcrush_params_t params, void *state);

typedef struct { float *signal; } nonlinear_samplerate_reduce_out_t;
typedef struct { float *signal; } nonlinear_samplerate_reduce_in_t;
typedef struct { float factor; } nonlinear_samplerate_reduce_params_t;
typedef struct { float last_val; float counter; } nonlinear_samplerate_reduce_state_t;
void nonlinear_samplerate_reduce(nonlinear_samplerate_reduce_out_t out, nonlinear_samplerate_reduce_in_t in, nonlinear_samplerate_reduce_params_t params, nonlinear_samplerate_reduce_state_t *state);

#endif //DSP_ATOMS_H