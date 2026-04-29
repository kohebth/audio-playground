#ifndef AUDIO_PLAYGROUND_DSP_TYPES_H
#define AUDIO_PLAYGROUND_DSP_TYPES_H
#include <stdint.h>

typedef float *Signal;
typedef float *Spectrum;
typedef float *Buffer;

// Standard IO Structs for Atom API
// clang-format off
typedef struct { float *signal; }                        atom_mono_t;
typedef struct { float *signal_a; float *signal_b; }     atom_pair_t;
typedef struct { float *left; float *right; }            atom_stereo_t;
typedef struct { float *real; float *imag; }             atom_complex_t;
typedef struct { float *mid; float *side; }              atom_ms_t;
typedef struct { float *dry; float *wet; }               atom_wet_dry_t;
typedef struct { float *numerator; float *denominator; } atom_div_t;
// clang-format on

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

typedef struct { float *signal; }                                     generation_oscillator_out_t;
typedef struct { /* no members */ }                                   generation_oscillator_in_t;
typedef struct { float frequency; int waveform; float phase_offset; float sample_rate; } generation_oscillator_params_t;
typedef struct { float phase; }                                       generation_oscillator_state_t;

typedef struct { float *signal; }                   generation_noise_out_t;
typedef struct { /* no members */ }                 generation_noise_in_t;
typedef struct { float amplitude; int color; }      generation_noise_params_t;
typedef struct { uint32_t seed; float prev_value; } generation_noise_state_t;

typedef struct { float *signal; }                                           generation_envelope_out_t;
typedef struct { float *gate; }                                             generation_envelope_in_t;
typedef struct { float attack; float decay; float sustain; float release; float sample_rate; } generation_envelope_params_t;
typedef struct { float current_level; int stage; }                          generation_envelope_state_t;

typedef struct { float *signal; }                                     generation_lfo_out_t;
typedef struct { /* no members */ }                                   generation_lfo_in_t;
typedef struct { float frequency; int waveform; float phase_offset; float sample_rate; } generation_lfo_params_t;
typedef struct { float phase; }                                       generation_lfo_state_t;

typedef struct { float *signal; } generation_impulse_out_t;
typedef struct { /* no members */ } generation_impulse_in_t;
typedef struct { float interval; float sample_rate; } generation_impulse_params_t;
typedef struct { int counter; } generation_impulse_state_t;

typedef struct { float *signal; }   generation_dc_out_t;
typedef struct { /* no members */ } generation_dc_in_t;
typedef struct { float value; }     generation_dc_params_t;
typedef struct { /* no members */ } generation_dc_state_t;

/* 🔷 Gain & Amplitude */

typedef struct { float *signal; }                    amplitude_multiply_out_t;
typedef struct { float *signal_a; float *signal_b; } amplitude_multiply_in_t;
typedef struct { /* no members */ }                  amplitude_multiply_params_t;
typedef struct { /* no members */ }                  amplitude_multiply_state_t;

typedef struct { float *signal; }                        amplitude_divide_out_t;
typedef struct { float *numerator; float *denominator; } amplitude_divide_in_t;
typedef struct { float epsilon; }                        amplitude_divide_params_t;
typedef struct { /* no members */ }                      amplitude_divide_state_t;

typedef struct { float *signal; }               amplitude_smooth_out_t;
typedef struct { float *signal; }               amplitude_smooth_in_t;
typedef struct { float attack; float release; float sample_rate; } amplitude_smooth_params_t;
typedef struct { float prev_value; }            amplitude_smooth_state_t;

typedef struct { float *signal; }   amplitude_clip_hard_out_t;
typedef struct { float *signal; }   amplitude_clip_hard_in_t;
typedef struct { float threshold; } amplitude_clip_hard_params_t;
typedef struct { /* no members */ } amplitude_clip_hard_state_t;

typedef struct { float *signal; }              amplitude_clip_soft_out_t;
typedef struct { float *signal; }              amplitude_clip_soft_in_t;
typedef struct { float threshold; int curve; } amplitude_clip_soft_params_t;
typedef struct { /* no members */ }            amplitude_clip_soft_state_t;


typedef struct { float *signal; }                amplitude_normalize_out_t;
typedef struct { float *signal; }                amplitude_normalize_in_t;
typedef struct { float target_level; int mode; } amplitude_normalize_params_t;
typedef struct { float running_peak; }           amplitude_normalize_state_t;

typedef struct { float *signal; }                    amplitude_add_out_t;
typedef struct { float *signal_a; float *signal_b; } amplitude_add_in_t;
typedef struct { /* no members */ }                  amplitude_add_params_t;
typedef struct { /* no members */ }                  amplitude_add_state_t;

typedef struct { float *signal; }                    amplitude_subtract_out_t;
typedef struct { float *signal_a; float *signal_b; } amplitude_subtract_in_t;
typedef struct { /* no members */ }                  amplitude_subtract_params_t;
typedef struct { /* no members */ }                  amplitude_subtract_state_t;

typedef struct { float *signal; }     amplitude_accumulate_out_t;
typedef struct { float *signal; }     amplitude_accumulate_in_t;
typedef struct { /* no members */ }   amplitude_accumulate_params_t;
typedef struct { float accumulator; } amplitude_accumulate_state_t;

/* 🔷 Delay & Memory */

typedef struct { float *signal; }     delay_unit_out_t;
typedef struct { float *signal; }     delay_unit_in_t;
typedef struct { /* no members */ }   delay_unit_params_t;
typedef struct { float prev_sample; } delay_unit_state_t;

typedef struct { float *signal; }                delay_line_out_t;
typedef struct { float *signal; }                delay_line_in_t;
typedef struct { int length; }                   delay_line_params_t;
typedef struct { float *buffer; int write_pos; } delay_line_state_t;

typedef struct { float *signal; }                          delay_fractional_out_t;
typedef struct { float *signal; }                          delay_fractional_in_t;
typedef struct { float delay_samples; int interpolation; } delay_fractional_params_t;
typedef struct { float *buffer; int write_pos; }           delay_fractional_state_t;

typedef struct { float *signal; }                   delay_tap_feedback_out_t;
typedef struct { float *buffer; int tap_position; } delay_tap_feedback_in_t;
typedef struct { float coefficient; }               delay_tap_feedback_params_t;
typedef struct { /* no members */ }                 delay_tap_feedback_state_t;

typedef struct { float *signal; }                   delay_tap_feedforward_out_t;
typedef struct { float *buffer; int tap_position; } delay_tap_feedforward_in_t;
typedef struct { float coefficient; }               delay_tap_feedforward_params_t;
typedef struct { /* no members */ }                 delay_tap_feedforward_state_t;


/* 🔷 Filtering */

typedef struct { float *signal; }                  filter_fir_out_t;
typedef struct { float *signal; }                  filter_fir_in_t;
typedef struct { float *kernel; int kernel_size; } filter_fir_params_t;
typedef struct { float *buffer; int write_pos; }   filter_fir_state_t;

typedef struct { float *signal; }                                    filter_biquad_out_t;
typedef struct { float *signal; }                                    filter_biquad_in_t;
typedef struct { float b0; float b1; float b2; float a1; float a2; } filter_biquad_params_t;
typedef struct { float z1; float z2; }                               filter_biquad_state_t;

typedef struct { float *signal; }                       filter_dc_block_out_t;
typedef struct { float *signal; }                       filter_dc_block_in_t;
typedef struct { float coefficient; }                   filter_dc_block_params_t;
typedef struct { float prev_input; float prev_output; } filter_dc_block_state_t;

typedef struct { float *signal; }                        filter_comb_ff_out_t;
typedef struct { float *signal; }                        filter_comb_ff_in_t;
typedef struct { int delay_samples; float coefficient; } filter_comb_ff_params_t;
typedef struct { float *buffer; int write_pos; }         filter_comb_ff_state_t;

typedef struct { float *signal; }                        filter_comb_fb_out_t;
typedef struct { float *signal; }                        filter_comb_fb_in_t;
typedef struct { int delay_samples; float coefficient; } filter_comb_fb_params_t;
typedef struct { float *buffer; int write_pos; }         filter_comb_fb_state_t;

typedef struct { float *signal; }                        filter_allpass_out_t;
typedef struct { float *signal; }                        filter_allpass_in_t;
typedef struct { int delay_samples; float coefficient; } filter_allpass_params_t;
typedef struct { float *buffer; int write_pos; }         filter_allpass_state_t;

typedef struct { float *signal; }     filter_integrate_out_t;
typedef struct { float *signal; }     filter_integrate_in_t;
typedef struct { /* no members */ }   filter_integrate_params_t;
typedef struct { float accumulator; } filter_integrate_state_t;

typedef struct { float *signal; }     filter_differentiate_out_t;
typedef struct { float *signal; }     filter_differentiate_in_t;
typedef struct { /* no members */ }   filter_differentiate_params_t;
typedef struct { float prev_sample; } filter_differentiate_state_t;

/* 🔷 Detection & Analysis */

typedef struct { float *level; }                detect_peak_out_t;
typedef struct { float *signal; }               detect_peak_in_t;
typedef struct { float attack; float release; float sample_rate; } detect_peak_params_t;
typedef struct { float prev_peak; }             detect_peak_state_t;

typedef struct { float *envelope; }             detect_envelope_out_t;
typedef struct { float *signal; }               detect_envelope_in_t;
typedef struct { float attack; float release; float sample_rate; } detect_envelope_params_t;
typedef struct { float prev_envelope; }         detect_envelope_state_t;

typedef struct { float *gate; }     detect_threshold_out_t;
typedef struct { float *signal; }   detect_threshold_in_t;
typedef struct { float threshold; } detect_threshold_params_t;
typedef struct { /* no members */ } detect_threshold_state_t;

typedef struct { float *level; }                            detect_rms_out_t;
typedef struct { float *signal; }                           detect_rms_in_t;
typedef struct { int window_size; }                         detect_rms_params_t;
typedef struct { float *buffer; int write_pos; float sum; } detect_rms_state_t;

typedef struct { float *trigger; }    detect_zero_crossing_out_t;
typedef struct { float *signal; }     detect_zero_crossing_in_t;
typedef struct { /* no members */ }   detect_zero_crossing_params_t;
typedef struct { float prev_sample; } detect_zero_crossing_state_t;

typedef struct { float *slope; }      detect_slope_out_t;
typedef struct { float *signal; }     detect_slope_in_t;
typedef struct { /* no members */ }   detect_slope_params_t;
typedef struct { float prev_sample; } detect_slope_state_t;

typedef struct { float *correlation; }           detect_autocorrelate_out_t;
typedef struct { float *signal; }                detect_autocorrelate_in_t;
typedef struct { int max_lag; }                  detect_autocorrelate_params_t;
typedef struct { float *buffer; int write_pos; } detect_autocorrelate_state_t;

/* 🔷 Modulation */

typedef struct { float *signal; }                   modulation_phase_out_t;
typedef struct { float *signal; float *modulator; } modulation_phase_in_t;
typedef struct { float depth; }                     modulation_phase_params_t;
typedef struct { float *buffer; int write_pos; }    modulation_phase_state_t;

typedef struct { float *signal; }                   modulation_ring_out_t;
typedef struct { float *signal; float *modulator; } modulation_ring_in_t;
typedef struct { /* no members */ }                 modulation_ring_params_t;
typedef struct { /* no members */ }                 modulation_ring_state_t;

typedef struct { float *signal; }                   modulation_amplitude_out_t;
typedef struct { float *signal; float *modulator; } modulation_amplitude_in_t;
typedef struct { float depth; }                     modulation_amplitude_params_t;
typedef struct { /* no members */ }                 modulation_amplitude_state_t;

typedef struct { float *signal; }                                     modulation_frequency_out_t;
typedef struct { float *signal; float *modulator; }                   modulation_frequency_in_t;
typedef struct { float depth; }                                       modulation_frequency_params_t;
typedef struct { float *buffer; int write_pos; float current_delay; } modulation_frequency_state_t;

typedef struct { float *signal; }                  modulation_scrub_out_t;
typedef struct { float *buffer; float *position; } modulation_scrub_in_t;
typedef struct { int buffer_size; }                modulation_scrub_params_t;
typedef struct { /* no members */ }                modulation_scrub_state_t;

/* 🔷 Interpolation */

typedef struct { float *signal; }                              interpolation_linear_out_t;
typedef struct { float *signal_a; float *signal_b; float *t; } interpolation_linear_in_t;
typedef struct { /* no members */ }                            interpolation_linear_params_t;
typedef struct { /* no members */ }                            interpolation_linear_state_t;

typedef struct { float *signal; } interpolation_cubic_out_t;
typedef struct { float *signal_n1; float *signal_a; float *signal_b; float *signal_c; float *t; } interpolation_cubic_in_t;
typedef struct { /* no members */ }                                                               interpolation_cubic_params_t;
typedef struct { /* no members */ }                                                               interpolation_cubic_state_t;

typedef struct { float *signal; }                  interpolation_sinc_out_t;
typedef struct { float *buffer; float *position; } interpolation_sinc_in_t;
typedef struct { int num_taps; }                   interpolation_sinc_params_t;
typedef struct { float *taps; }                    interpolation_sinc_state_t;

typedef struct { float *signal; }                interpolation_lagrange_out_t;
typedef struct { float *samples; float *t; }     interpolation_lagrange_in_t;
typedef struct { int order; }                    interpolation_lagrange_params_t;
typedef struct { float *buffer; int write_pos; } interpolation_lagrange_state_t;

/* 🔷 Sample Rate Conversion */

typedef struct { float *signal; }   src_upsample_out_t;
typedef struct { float *signal; }   src_upsample_in_t;
typedef struct { int factor; }      src_upsample_params_t;
typedef struct { /* no members */ } src_upsample_state_t;

typedef struct { float *signal; }   src_downsample_out_t;
typedef struct { float *signal; }   src_downsample_in_t;
typedef struct { int factor; }      src_downsample_params_t;
typedef struct { /* no members */ } src_downsample_state_t;

typedef struct { float *signal; }      src_antialias_out_t;
typedef struct { float *signal; }      src_antialias_in_t;
typedef struct { float cutoff; float sample_rate; }       src_antialias_params_t;
typedef struct { float z1; float z2; } src_antialias_state_t;

typedef struct { float *signal; }      src_antiimage_out_t;
typedef struct { float *signal; }      src_antiimage_in_t;
typedef struct { float cutoff; float sample_rate; }       src_antiimage_params_t;
typedef struct { float z1; float z2; } src_antiimage_state_t;

typedef struct { float *signal; }                  src_convert_format_out_t;
typedef struct { float *signal; }                  src_convert_format_in_t;
typedef struct { int from_format; int to_format; } src_convert_format_params_t;
typedef struct { /* no members */ }                src_convert_format_state_t;

/* 🔷 Frequency Domain */

typedef struct { float *real; float *imag; } freq_fft_out_t;
typedef struct { float *signal; }            freq_fft_in_t;
typedef struct { int block_size; }           freq_fft_params_t;
typedef struct { /* no members */ }          freq_fft_state_t;

typedef struct { float *signal; }            freq_ifft_out_t;
typedef struct { float *real; float *imag; } freq_ifft_in_t;
typedef struct { int block_size; }           freq_ifft_params_t;
typedef struct { /* no members */ }          freq_ifft_state_t;

typedef struct { float *signal; }                   freq_window_out_t;
typedef struct { float *signal; }                   freq_window_in_t;
typedef struct { int window_type; int block_size; } freq_window_params_t;
typedef struct { /* no members */ }                 freq_window_state_t;

typedef struct { float *real; float *imag; }                                   freq_multiply_out_t;
typedef struct { float *real_a; float *imag_a; float *real_b; float *imag_b; } freq_multiply_in_t;
typedef struct { int block_size; }                                             freq_multiply_params_t;
typedef struct { /* no members */ }                                            freq_multiply_state_t;

typedef struct { float *signal; }                freq_overlap_add_out_t;
typedef struct { float *frame; }                 freq_overlap_add_in_t;
typedef struct { int block_size; int hop_size; } freq_overlap_add_params_t;
typedef struct { float *buffer; }                freq_overlap_add_state_t;

typedef struct { float *frame; }                 freq_overlap_save_out_t;
typedef struct { float *signal; }                freq_overlap_save_in_t;
typedef struct { int block_size; int hop_size; } freq_overlap_save_params_t;
typedef struct { float *buffer; int write_pos; } freq_overlap_save_state_t;

/* 🔷 Mixing & Routing */

typedef struct { float *signal; }                    mix_crossfade_out_t;
typedef struct { float *signal_a; float *signal_b; } mix_crossfade_in_t;
typedef struct { float t; }                          mix_crossfade_params_t;
typedef struct { /* no members */ }                  mix_crossfade_state_t;

typedef struct { float *signal; }          mix_wet_dry_out_t;
typedef struct { float *dry; float *wet; } mix_wet_dry_in_t;
typedef struct { float mix; }              mix_wet_dry_params_t;
typedef struct { /* no members */ }        mix_wet_dry_state_t;

typedef struct { float **signals; }                               mix_matrix_out_t;
typedef struct { float **signals; }                               mix_matrix_in_t;
typedef struct { float **coefficients; int num_in; int num_out; } mix_matrix_params_t;
typedef struct { /* no members */ }                               mix_matrix_state_t;

typedef struct { float *left; float *right; } mix_pan_stereo_out_t;
typedef struct { float *signal; }             mix_pan_stereo_in_t;
typedef struct { float position; }            mix_pan_stereo_params_t;
typedef struct { /* no members */ }           mix_pan_stereo_state_t;

typedef struct { float *mid; float *side; }   mix_encode_ms_out_t;
typedef struct { float *left; float *right; } mix_encode_ms_in_t;
typedef struct { /* no members */ }           mix_encode_ms_params_t;
typedef struct { /* no members */ }           mix_encode_ms_state_t;

typedef struct { float *left; float *right; } mix_decode_ms_out_t;
typedef struct { float *mid; float *side; }   mix_decode_ms_in_t;
typedef struct { /* no members */ }           mix_decode_ms_params_t;
typedef struct { /* no members */ }           mix_decode_ms_state_t;

/* 🔷 Nonlinear / Distortion */

typedef struct { float *signal; }                         nonlinear_waveshape_out_t;
typedef struct { float *signal; }                         nonlinear_waveshape_in_t;
typedef struct { float *transfer_table; int table_size; } nonlinear_waveshape_params_t;
typedef struct { /* no members */ }                       nonlinear_waveshape_state_t;

typedef struct { float *signal; }   nonlinear_bitcrush_out_t;
typedef struct { float *signal; }   nonlinear_bitcrush_in_t;
typedef struct { float bit_depth; } nonlinear_bitcrush_params_t;
typedef struct { /* no members */ } nonlinear_bitcrush_state_t;

typedef struct { float *signal; }                 nonlinear_samplerate_reduce_out_t;
typedef struct { float *signal; }                 nonlinear_samplerate_reduce_in_t;
typedef struct { float factor; }                  nonlinear_samplerate_reduce_params_t;
typedef struct { float last_val; float counter; } nonlinear_samplerate_reduce_state_t;

#endif // AUDIO_PLAYGROUND_DSP_TYPES_H
