#ifndef AUDIO_PLAYGROUND_DSP_TYPES_H
#define AUDIO_PLAYGROUND_DSP_TYPES_H

#include <atom/atom_definitions.h>

#include <stdint.h>

typedef float *Signal;
typedef float *Spectrum;
typedef float *Buffer;

/* I/O profiles describe C member layouts, not YAML binding contracts. */
// clang-format off
#define APG_IO_FIELDS_EMPTY                 {}
#define APG_IO_FIELDS_SIGNAL                { float *signal; }
#define APG_IO_FIELDS_SIGNAL_PAIR           { float *signal_a; float *signal_b; }
#define APG_IO_FIELDS_STEREO                { float *left; float *right; }
#define APG_IO_FIELDS_COMPLEX               { float *real; float *imag; }
#define APG_IO_FIELDS_MS                    { float *mid; float *side; }
#define APG_IO_FIELDS_WET_DRY               { float *dry; float *wet; }
#define APG_IO_FIELDS_DIVISION              { float *numerator; float *denominator; }
#define APG_IO_FIELDS_CORRELATION           { float *correlation; }
#define APG_IO_FIELDS_PITCH                 { float *pitch; }
#define APG_IO_FIELDS_ENVELOPE              { float *envelope; }
#define APG_IO_FIELDS_LEVEL                 { float *level; }
#define APG_IO_FIELDS_SLOPE                 { float *slope; }
#define APG_IO_FIELDS_GATE                  { float *gate; }
#define APG_IO_FIELDS_TRIGGER               { float *trigger; }
#define APG_IO_FIELDS_BUFFER_TAP            { float *buffer; int tap_position; }
#define APG_IO_FIELDS_COMPLEX_PAIR          { float *real_a; float *imag_a; float *real_b; float *imag_b; }
#define APG_IO_FIELDS_FRAME                 { float *frame; }
#define APG_IO_FIELDS_SIGNAL_PITCH_SHIFT    { float *signal; float *pitch_shift; }
#define APG_IO_FIELDS_FREQUENCY             { float *frequency; }
#define APG_IO_FIELDS_INTERPOLATION_CUBIC   { float *signal_n1; float *signal_a; float *signal_b; float *signal_c; float *t; }
#define APG_IO_FIELDS_SAMPLES_T             { float *samples; float *t; }
#define APG_IO_FIELDS_INTERPOLATION_LINEAR  { float *signal_a; float *signal_b; float *t; }
#define APG_IO_FIELDS_BUFFER_POSITION       { float *buffer; float *position; }
#define APG_IO_FIELDS_SIGNAL_MATRIX         { float **signals; }
#define APG_IO_FIELDS_SIGNAL_CUTOFF         { float *signal; float *cutoff; }
#define APG_IO_FIELDS_SIGNAL_DELAY          { float *signal; float *delay; }
#define APG_IO_FIELDS_SIGNAL_GATE           { float *signal; float *gate; }
#define APG_IO_FIELDS_SIGNAL_MODULATOR      { float *signal; float *modulator; }
// clang-format on

// Standard IO structs for the Atom API.
// clang-format off
typedef struct APG_IO_FIELDS_SIGNAL      atom_mono_t;
typedef struct APG_IO_FIELDS_SIGNAL_PAIR atom_pair_t;
typedef struct APG_IO_FIELDS_STEREO      atom_stereo_t;
typedef struct APG_IO_FIELDS_COMPLEX     atom_complex_t;
typedef struct APG_IO_FIELDS_MS          atom_ms_t;
typedef struct APG_IO_FIELDS_WET_DRY     atom_wet_dry_t;
typedef struct APG_IO_FIELDS_DIVISION    atom_div_t;
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

/* Each row is: atom, output profile, input profile, parameter fields, state fields. */
// clang-format off
#define APG_DSP_TYPE_TABLE(X) \
    /* Amplitude */ \
    X(amplitude_accumulate, SIGNAL, SIGNAL, {}, { float accumulator; }) \
    X(amplitude_latch, SIGNAL, SIGNAL_GATE, { float threshold; }, { float latched_value; int prev_gate; }) \
    X(amplitude_add, SIGNAL, SIGNAL_PAIR, {}, {}) \
    X(amplitude_clip_hard, SIGNAL, SIGNAL, { float threshold; }, {}) \
    X(amplitude_clip_soft, SIGNAL, SIGNAL, { float threshold; int curve; }, {}) \
    X(amplitude_divide, SIGNAL, DIVISION, { float epsilon; }, {}) \
    X(amplitude_multiply, SIGNAL, SIGNAL_PAIR, {}, {}) \
    X(amplitude_normalize, SIGNAL, SIGNAL, { float target_level; int mode; }, { float running_peak; }) \
    X(amplitude_smooth, SIGNAL, SIGNAL, { float attack; float release; float sample_rate; }, { float prev_value; }) \
    X(amplitude_subtract, SIGNAL, SIGNAL_PAIR, {}, {}) \
    /* Delay and memory */ \
    X(delay_fractional, SIGNAL, SIGNAL, { float delay_samples; int interpolation; }, { float *buffer; uint32_t buffer_len; int write_pos; }) \
    X(delay_line, SIGNAL, SIGNAL, { int length; }, { float *buffer; uint32_t buffer_len; int write_pos; }) \
    X(delay_tap_feedback, SIGNAL, BUFFER_TAP, { float coefficient; }, {}) \
    X(delay_tap_feedforward, SIGNAL, BUFFER_TAP, { float coefficient; }, {}) \
    X(delay_unit, SIGNAL, SIGNAL, {}, { float prev_sample; }) \
    /* Detection and analysis */ \
    X(detect_autocorrelate, CORRELATION, SIGNAL, { int max_lag; }, { float *buffer; uint32_t buffer_len; int write_pos; }) \
    X(detect_pitch, PITCH, SIGNAL, { int max_lag; float sample_rate; }, { float *buffer; uint32_t buffer_len; int write_pos; }) \
    X(detect_envelope, ENVELOPE, SIGNAL, { float attack; float release; float sample_rate; }, { float prev_envelope; }) \
    X(detect_peak, LEVEL, SIGNAL, { float attack; float release; float sample_rate; }, { float prev_peak; }) \
    X(detect_rms, LEVEL, SIGNAL, { int window_size; }, { float *buffer; uint32_t buffer_len; int write_pos; float sum; }) \
    X(detect_slope, SLOPE, SIGNAL, {}, { float prev_sample; }) \
    X(detect_threshold, GATE, SIGNAL, { float threshold; }, {}) \
    X(detect_zero_crossing, TRIGGER, SIGNAL, {}, { float prev_sample; }) \
    /* Filtering */ \
    X(filter_allpass, SIGNAL, SIGNAL, { int delay_samples; float coefficient; }, { float *buffer; uint32_t buffer_len; int write_pos; }) \
    X(filter_biquad_coefficients, SIGNAL, SIGNAL, { float b0; float b1; float b2; float a1; float a2; }, { float z1; float z2; }) \
    X(filter_biquad, SIGNAL, SIGNAL_CUTOFF, { float cutoff; float q; int mode; float sample_rate; float smoothing_ms; }, { float z1; float z2; float current_cutoff; float current_q; float current_b0; float current_b1; float current_b2; float current_a1; float current_a2; }) \
    X(filter_comb_fb, SIGNAL, SIGNAL_DELAY, { int delay_samples; float coefficient; }, { float *buffer; uint32_t buffer_len; int write_pos; }) \
    X(filter_comb_ff, SIGNAL, SIGNAL, { int delay_samples; float coefficient; }, { float *buffer; uint32_t buffer_len; int write_pos; }) \
    X(filter_dc_block, SIGNAL, SIGNAL, { float coefficient; }, { float prev_input; float prev_output; }) \
    X(filter_differentiate, SIGNAL, SIGNAL, {}, { float prev_sample; }) \
    X(filter_fir, SIGNAL, SIGNAL, { float *kernel; int kernel_size; }, { float *buffer; uint32_t buffer_len; int write_pos; }) \
    X(filter_integrate, SIGNAL, SIGNAL, {}, { float accumulator; }) \
    /* Frequency domain */ \
    X(freq_fft, COMPLEX, SIGNAL, { int block_size; }, { float *workspace; uint32_t buffer_len; }) \
    X(freq_ifft, SIGNAL, COMPLEX, { int block_size; }, { float *workspace; uint32_t buffer_len; }) \
    X(freq_multiply, COMPLEX, COMPLEX_PAIR, { int block_size; }, {}) \
    X(freq_overlap_add, SIGNAL, FRAME, { int block_size; int hop_size; }, { float *buffer; }) \
    X(freq_overlap_save, FRAME, SIGNAL, { int block_size; int hop_size; }, { float *buffer; int write_pos; }) \
    X(freq_window, SIGNAL, SIGNAL, { int window_type; int block_size; }, {}) \
    X(freq_shift, SIGNAL, SIGNAL_PITCH_SHIFT, { int block_size; }, { float *window; float *real; float *imag; int write_pos; float read_ptr; }) \
    /* Signal generation */ \
    X(generation_dc, SIGNAL, EMPTY, { float value; }, {}) \
    X(generation_envelope, SIGNAL, GATE, { float attack; float decay; float sustain; float release; float sample_rate; }, { float current_level; int stage; }) \
    X(generation_impulse, SIGNAL, EMPTY, { float interval; float sample_rate; }, { int counter; }) \
    X(generation_lfo, SIGNAL, EMPTY, { float frequency; int waveform; float phase_offset; float sample_rate; }, { float phase; }) \
    X(generation_noise, SIGNAL, EMPTY, { float amplitude; int color; }, { uint32_t seed; float prev_value; }) \
    X(generation_oscillator, SIGNAL, FREQUENCY, { float frequency; int waveform; float phase_offset; float sample_rate; }, { float phase; }) \
    /* Interpolation */ \
    X(interpolation_cubic, SIGNAL, INTERPOLATION_CUBIC, {}, {}) \
    X(interpolation_lagrange, SIGNAL, SAMPLES_T, { int order; }, { float *buffer; int write_pos; }) \
    X(interpolation_linear, SIGNAL, INTERPOLATION_LINEAR, {}, {}) \
    X(interpolation_sinc, SIGNAL, BUFFER_POSITION, { int num_taps; }, { float *taps; }) \
    /* Mixing and routing */ \
    X(mix_crossfade, SIGNAL, SIGNAL_PAIR, { float t; }, {}) \
    X(mix_decode_ms, STEREO, MS, {}, {}) \
    X(mix_encode_ms, MS, STEREO, {}, {}) \
    X(mix_matrix, SIGNAL_MATRIX, SIGNAL_MATRIX, { float **coefficients; int num_in; int num_out; }, {}) \
    X(mix_pan_stereo, STEREO, SIGNAL, { float position; }, {}) \
    X(mix_wet_dry, SIGNAL, WET_DRY, { float mix; }, {}) \
    /* Modulation */ \
    X(modulation_amplitude, SIGNAL, SIGNAL_MODULATOR, { float depth; }, {}) \
    X(modulation_frequency, SIGNAL, SIGNAL_MODULATOR, { float depth; }, { float *buffer; uint32_t buffer_len; int write_pos; float current_delay; }) \
    X(modulation_phase, SIGNAL, SIGNAL_MODULATOR, { float depth; }, { float *buffer; uint32_t buffer_len; int write_pos; }) \
    X(modulation_ring, SIGNAL, SIGNAL_MODULATOR, {}, {}) \
    X(modulation_scrub, SIGNAL, BUFFER_POSITION, { int buffer_size; }, {}) \
    /* Nonlinear and distortion */ \
    X(nonlinear_bitcrush, SIGNAL, SIGNAL, { float bit_depth; }, {}) \
    X(nonlinear_sample_hold, SIGNAL, SIGNAL, { float factor; }, { float last_val; float counter; }) \
    X(nonlinear_waveshape, SIGNAL, SIGNAL, { float *transfer_table; int table_size; }, {}) \
    /* Sample-rate conversion */ \
    X(src_antialias, SIGNAL, SIGNAL, { float cutoff; float sample_rate; }, { float z1; float z2; }) \
    X(src_antiimage, SIGNAL, SIGNAL, { float cutoff; float sample_rate; }, { float z1; float z2; }) \
    X(src_convert_format, SIGNAL, SIGNAL, { int from_format; int to_format; }, {}) \
    X(src_downsample, SIGNAL, SIGNAL, { int factor; }, {}) \
    X(src_upsample, SIGNAL, SIGNAL, { int factor; }, {}) \
    /* Frequency domain */ \
    X(freq_quantize, SIGNAL, SIGNAL, { float unused; }, { float unused; })
// clang-format on

#define APG_EXPAND_IO_FIELDS(profile)       APG_EXPAND_IO_FIELDS_INNER(profile)
#define APG_EXPAND_IO_FIELDS_INNER(profile) APG_IO_FIELDS_##profile

#define APG_DECLARE_DSP_TYPES(atom_name, out_profile, in_profile, params_fields, state_fields) \
    typedef struct APG_EXPAND_IO_FIELDS(out_profile) atom_name##_out_t;                        \
    typedef struct APG_EXPAND_IO_FIELDS(in_profile) atom_name##_in_t;                          \
    typedef struct params_fields atom_name##_params_t;                                         \
    typedef struct state_fields  atom_name##_state_t;

APG_DSP_TYPE_TABLE(APG_DECLARE_DSP_TYPES)

#define APG_COUNT_DSP_TYPE_ROW(atom_name, out_profile, in_profile, params_fields, state_fields) +1
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
#undef APG_EXPAND_IO_FIELDS_INNER
#undef APG_EXPAND_IO_FIELDS
#undef APG_DSP_TYPE_TABLE
#undef APG_IO_FIELDS_SIGNAL_MODULATOR
#undef APG_IO_FIELDS_SIGNAL_GATE
#undef APG_IO_FIELDS_SIGNAL_DELAY
#undef APG_IO_FIELDS_SIGNAL_CUTOFF
#undef APG_IO_FIELDS_SIGNAL_MATRIX
#undef APG_IO_FIELDS_BUFFER_POSITION
#undef APG_IO_FIELDS_INTERPOLATION_LINEAR
#undef APG_IO_FIELDS_SAMPLES_T
#undef APG_IO_FIELDS_INTERPOLATION_CUBIC
#undef APG_IO_FIELDS_FREQUENCY
#undef APG_IO_FIELDS_SIGNAL_PITCH_SHIFT
#undef APG_IO_FIELDS_FRAME
#undef APG_IO_FIELDS_COMPLEX_PAIR
#undef APG_IO_FIELDS_BUFFER_TAP
#undef APG_IO_FIELDS_TRIGGER
#undef APG_IO_FIELDS_GATE
#undef APG_IO_FIELDS_SLOPE
#undef APG_IO_FIELDS_LEVEL
#undef APG_IO_FIELDS_ENVELOPE
#undef APG_IO_FIELDS_PITCH
#undef APG_IO_FIELDS_CORRELATION
#undef APG_IO_FIELDS_DIVISION
#undef APG_IO_FIELDS_WET_DRY
#undef APG_IO_FIELDS_MS
#undef APG_IO_FIELDS_COMPLEX
#undef APG_IO_FIELDS_STEREO
#undef APG_IO_FIELDS_SIGNAL_PAIR
#undef APG_IO_FIELDS_SIGNAL
#undef APG_IO_FIELDS_EMPTY

#endif // AUDIO_PLAYGROUND_DSP_TYPES_H
