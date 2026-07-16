#ifndef AUDIO_PLAYGROUND_DSP_TYPES_H
#define AUDIO_PLAYGROUND_DSP_TYPES_H

#include <atom/atom_definitions.h>

#include <stdint.h>

typedef float *Signal;
typedef float *Spectrum;
typedef float *Buffer;

// Standard IO structs for the Atom API.
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

#define APG_DETECT_RMS_CAPACITY             4096u
#define APG_DETECT_AUTOCORRELATION_CAPACITY 1024u
#define APG_MODULATION_DELAY_CAPACITY       4096u

/* Each row is: atom, output fields, input fields, parameter fields, state fields. */
// clang-format off
#define APG_DSP_TYPE_TABLE(X) \
    /* Amplitude */ \
    X(amplitude_accumulate, { float *signal; }, { float *signal; }, {}, { float accumulator; }) \
    X(amplitude_latch, { float *signal; }, { float *signal; float *gate; }, { float threshold; }, { float latched_value; int prev_gate; }) \
    X(amplitude_add, { float *signal; }, { float *signal_a; float *signal_b; }, {}, {}) \
    X(amplitude_clip_hard, { float *signal; }, { float *signal; }, { float threshold; }, {}) \
    X(amplitude_clip_soft, { float *signal; }, { float *signal; }, { float threshold; int curve; }, {}) \
    X(amplitude_divide, { float *signal; }, { float *numerator; float *denominator; }, { float epsilon; }, {}) \
    X(amplitude_multiply, { float *signal; }, { float *signal_a; float *signal_b; }, {}, {}) \
    X(amplitude_normalize, { float *signal; }, { float *signal; }, { float target_level; int mode; }, { float running_peak; }) \
    X(amplitude_smooth, { float *signal; }, { float *signal; }, { float attack; float release; float sample_rate; }, { float prev_value; }) \
    X(amplitude_subtract, { float *signal; }, { float *signal_a; float *signal_b; }, {}, {}) \
    /* Delay and memory */ \
    X(delay_fractional, { float *signal; }, { float *signal; }, { float delay_samples; int interpolation; }, { float *buffer; uint32_t buffer_len; int write_pos; }) \
    X(delay_line, { float *signal; }, { float *signal; }, { int length; }, { float *buffer; uint32_t buffer_len; int write_pos; }) \
    X(delay_tap_feedback, { float *signal; }, { float *buffer; int tap_position; }, { float coefficient; }, {}) \
    X(delay_tap_feedforward, { float *signal; }, { float *buffer; int tap_position; }, { float coefficient; }, {}) \
    X(delay_unit, { float *signal; }, { float *signal; }, {}, { float prev_sample; }) \
    /* Detection and analysis */ \
    X(detect_autocorrelate, { float *correlation; }, { float *signal; }, { int max_lag; }, { float *buffer; uint32_t buffer_len; int write_pos; }) \
    X(detect_pitch, { float *pitch; }, { float *signal; }, { int max_lag; float sample_rate; }, { float *buffer; uint32_t buffer_len; int write_pos; }) \
    X(detect_envelope, { float *envelope; }, { float *signal; }, { float attack; float release; float sample_rate; }, { float prev_envelope; }) \
    X(detect_peak, { float *level; }, { float *signal; }, { float attack; float release; float sample_rate; }, { float prev_peak; }) \
    X(detect_rms, { float *level; }, { float *signal; }, { int window_size; }, { float *buffer; uint32_t buffer_len; int write_pos; float sum; }) \
    X(detect_slope, { float *slope; }, { float *signal; }, {}, { float prev_sample; }) \
    X(detect_threshold, { float *gate; }, { float *signal; }, { float threshold; }, {}) \
    X(detect_zero_crossing, { float *trigger; }, { float *signal; }, {}, { float prev_sample; }) \
    /* Filtering */ \
    X(filter_allpass, { float *signal; }, { float *signal; }, { int delay_samples; float coefficient; }, { float *buffer; uint32_t buffer_len; int write_pos; }) \
    X(filter_biquad_coefficients, { float *signal; }, { float *signal; }, { float b0; float b1; float b2; float a1; float a2; }, { float z1; float z2; }) \
    X(filter_biquad, { float *signal; }, { float *signal; float *cutoff; }, { float cutoff; float q; int mode; float sample_rate; float smoothing_ms; }, { float z1; float z2; float current_cutoff; float current_q; float current_b0; float current_b1; float current_b2; float current_a1; float current_a2; }) \
    X(filter_comb_fb, { float *signal; }, { float *signal; float *delay; }, { int delay_samples; float coefficient; }, { float *buffer; uint32_t buffer_len; int write_pos; }) \
    X(filter_comb_ff, { float *signal; }, { float *signal; }, { int delay_samples; float coefficient; }, { float *buffer; uint32_t buffer_len; int write_pos; }) \
    X(filter_dc_block, { float *signal; }, { float *signal; }, { float coefficient; }, { float prev_input; float prev_output; }) \
    X(filter_differentiate, { float *signal; }, { float *signal; }, {}, { float prev_sample; }) \
    X(filter_fir, { float *signal; }, { float *signal; }, { float *kernel; int kernel_size; }, { float *buffer; uint32_t buffer_len; int write_pos; }) \
    X(filter_integrate, { float *signal; }, { float *signal; }, {}, { float accumulator; }) \
    /* Frequency domain */ \
    X(freq_fft, { float *real; float *imag; }, { float *signal; }, { int block_size; }, { float *workspace; uint32_t buffer_len; }) \
    X(freq_ifft, { float *signal; }, { float *real; float *imag; }, { int block_size; }, { float *workspace; uint32_t buffer_len; }) \
    X(freq_multiply, { float *real; float *imag; }, { float *real_a; float *imag_a; float *real_b; float *imag_b; }, { int block_size; }, {}) \
    X(freq_overlap_add, { float *signal; }, { float *frame; }, { int block_size; int hop_size; }, { float *buffer; }) \
    X(freq_overlap_save, { float *frame; }, { float *signal; }, { int block_size; int hop_size; }, { float *buffer; int write_pos; }) \
    X(freq_window, { float *signal; }, { float *signal; }, { int window_type; int block_size; }, {}) \
    X(freq_shift, { float *signal; }, { float *signal; float *pitch_shift; }, { int block_size; }, { float *window; float *real; float *imag; int write_pos; float read_ptr; }) \
    /* Signal generation */ \
    X(generation_dc, { float *signal; }, {}, { float value; }, {}) \
    X(generation_envelope, { float *signal; }, { float *gate; }, { float attack; float decay; float sustain; float release; float sample_rate; }, { float current_level; int stage; }) \
    X(generation_impulse, { float *signal; }, {}, { float interval; float sample_rate; }, { int counter; }) \
    X(generation_lfo, { float *signal; }, {}, { float frequency; int waveform; float phase_offset; float sample_rate; }, { float phase; }) \
    X(generation_noise, { float *signal; }, {}, { float amplitude; int color; }, { uint32_t seed; float prev_value; }) \
    X(generation_oscillator, { float *signal; }, { float *frequency; }, { float frequency; int waveform; float phase_offset; float sample_rate; }, { float phase; }) \
    /* Interpolation */ \
    X(interpolation_cubic, { float *signal; }, { float *signal_n1; float *signal_a; float *signal_b; float *signal_c; float *t; }, {}, {}) \
    X(interpolation_lagrange, { float *signal; }, { float *samples; float *t; }, { int order; }, { float *buffer; int write_pos; }) \
    X(interpolation_linear, { float *signal; }, { float *signal_a; float *signal_b; float *t; }, {}, {}) \
    X(interpolation_sinc, { float *signal; }, { float *buffer; float *position; }, { int num_taps; }, { float *taps; }) \
    /* Mixing and routing */ \
    X(mix_crossfade, { float *signal; }, { float *signal_a; float *signal_b; }, { float t; }, {}) \
    X(mix_decode_ms, { float *left; float *right; }, { float *mid; float *side; }, {}, {}) \
    X(mix_encode_ms, { float *mid; float *side; }, { float *left; float *right; }, {}, {}) \
    X(mix_matrix, { float **signals; }, { float **signals; }, { float **coefficients; int num_in; int num_out; }, {}) \
    X(mix_pan_stereo, { float *left; float *right; }, { float *signal; }, { float position; }, {}) \
    X(mix_wet_dry, { float *signal; }, { float *dry; float *wet; }, { float mix; }, {}) \
    /* Modulation */ \
    X(modulation_amplitude, { float *signal; }, { float *signal; float *modulator; }, { float depth; }, {}) \
    X(modulation_frequency, { float *signal; }, { float *signal; float *modulator; }, { float depth; }, { float *buffer; uint32_t buffer_len; int write_pos; float current_delay; }) \
    X(modulation_phase, { float *signal; }, { float *signal; float *modulator; }, { float depth; }, { float *buffer; uint32_t buffer_len; int write_pos; }) \
    X(modulation_ring, { float *signal; }, { float *signal; float *modulator; }, {}, {}) \
    X(modulation_scrub, { float *signal; }, { float *buffer; float *position; }, { int buffer_size; }, {}) \
    /* Nonlinear and distortion */ \
    X(nonlinear_bitcrush, { float *signal; }, { float *signal; }, { float bit_depth; }, {}) \
    X(nonlinear_sample_hold, { float *signal; }, { float *signal; }, { float factor; }, { float last_val; float counter; }) \
    X(nonlinear_waveshape, { float *signal; }, { float *signal; }, { float *transfer_table; int table_size; }, {}) \
    /* Sample-rate conversion */ \
    X(src_antialias, { float *signal; }, { float *signal; }, { float cutoff; float sample_rate; }, { float z1; float z2; }) \
    X(src_antiimage, { float *signal; }, { float *signal; }, { float cutoff; float sample_rate; }, { float z1; float z2; }) \
    X(src_convert_format, { float *signal; }, { float *signal; }, { int from_format; int to_format; }, {}) \
    X(src_downsample, { float *signal; }, { float *signal; }, { int factor; }, {}) \
    X(src_upsample, { float *signal; }, { float *signal; }, { int factor; }, {}) \
    /* Frequency domain */ \
    X(freq_quantize, { float *signal; }, { float *signal; }, { float unused; }, { float unused; })
// clang-format on

#define APG_DECLARE_DSP_TYPES(atom_name, out_fields, in_fields, params_fields, state_fields) \
    typedef struct out_fields    atom_name##_out_t;                                          \
    typedef struct in_fields     atom_name##_in_t;                                           \
    typedef struct params_fields atom_name##_params_t;                                       \
    typedef struct state_fields  atom_name##_state_t;

APG_DSP_TYPE_TABLE(APG_DECLARE_DSP_TYPES)

#define APG_COUNT_DSP_TYPE_ROW(atom_name, out_fields, in_fields, params_fields, state_fields) +1
#define APG_COUNT_ATOM_DEFINITION(                                                         \
    atom_name, category, input_count, config_count, state_count, flags, maturity, dispatch \
)                                                                                          \
    +1
_Static_assert(
    (0 APG_DSP_TYPE_TABLE(APG_COUNT_DSP_TYPE_ROW)) == (0 APG_ATOM_DEFINITIONS(APG_COUNT_ATOM_DEFINITION)),
    "DSP type table must contain exactly one row per canonical atom"
);

#undef APG_COUNT_ATOM_DEFINITION
#undef APG_COUNT_DSP_TYPE_ROW
#undef APG_DECLARE_DSP_TYPES
#undef APG_DSP_TYPE_TABLE

#endif // AUDIO_PLAYGROUND_DSP_TYPES_H
