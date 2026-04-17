#ifndef DSP_ATOMS_H
#define DSP_ATOMS_H

#include <stdint.h>
#include <stddef.h>

typedef float *Signal;
typedef float *Spectrum;
typedef float *Buffer;

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

typedef struct {
    float attack;
    float decay;
    float sustain;
    float release;
    float gate;
    uint32_t sample_rate;
} EnvelopeParams;

typedef struct {
    float threshold;
} ThresholdParams;

typedef struct {
    float attack;
    float release;
    uint32_t sample_rate;
} DetectorParams;

typedef struct {
    uint32_t sample_rate;
    float frequency;
    WaveformType waveform;
    float phase;
} OscillatorParams;

typedef struct {
    float time;
    uint32_t sample_rate;
} SmoothParams;

typedef struct {
    float threshold;
} ClipParams;

typedef struct {
    float target_level;
    NormalizeMode mode;
} NormalizeParams;

typedef struct {
    uint32_t length;
    uint32_t sample_rate;
} DelayLineParams;

typedef struct {
    float delay_samples;
    InterpolationType interpolation;
} FractionalDelayParams;

typedef struct {
    float tap_position;
    float coefficient;
} TapParams;

typedef struct {
    float b0, b1, b2, a1, a2;
} BiquadParams;

typedef struct {
    float delay_samples;
    float coefficient;
} CombParams;

typedef struct {
    float coefficient;
} DcBlockParams;

typedef struct {
    uint32_t window_size;
} RmsParams;

typedef struct {
    uint32_t max_lag;
} AutocorrelateParams;

typedef struct {
    Buffer modulator;
    float depth;
} ModulationParams;

typedef struct {
    Buffer position_signal;
} ScrubParams;

typedef struct {
    float t;
} LinearInterpParams;

typedef struct {
    float sample_n1;
    float sample_a;
    float sample_b;
    float sample_c;
    float t;
} CubicInterpParams;

typedef struct {
    Buffer buffer;
    float position;
    uint32_t num_taps;
} SincInterpParams;

typedef struct {
    Buffer samples;
    uint32_t order;
    float t;
} LagrangeInterpParams;

typedef struct {
    uint32_t factor;
} SrcParams;

typedef struct {
    float cutoff;
    uint32_t sample_rate;
} FilterParams;

typedef struct {
    uint32_t block_size;
    WindowType window;
} FftParams;

typedef struct {
    Spectrum spectrum_a;
    Spectrum spectrum_b;
} FreqMultiplyParams;

typedef struct {
    Buffer *frames;
    uint32_t block_size;
    uint32_t hop_size;
} OverlapAddParams;

typedef struct {
    Buffer signal;
    uint32_t block_size;
    uint32_t hop_size;
} OverlapSaveParams;

typedef struct {
    float t;
} CrossfadeParams;

typedef struct {
    float mix;
} WetDryParams;

typedef struct {
    float **coefficients;
    uint32_t num_inputs;
    uint32_t num_outputs;
} MixMatrixParams;

typedef struct {
    float position;
} PanParams;

typedef struct {
    Buffer transfer_table;
} WaveshapeParams;

typedef struct {
    float bit_depth;
} BitcrushParams;

typedef struct {
    float reduction_factor;
} SampleRateReduceParams;

Signal generation_oscillator(OscillatorParams params);
Signal generation_noise(WaveformType color, uint32_t seed);
Signal generation_envelope(EnvelopeParams params);
Signal generation_lfo(OscillatorParams params);
Signal generation_impulse(uint32_t interval, uint32_t sample_rate);
Signal generation_dc(float value);

Signal amplitude_multiply(Signal signal, float scalar);
Signal amplitude_add(Signal signal_a, Signal signal_b);
Signal amplitude_subtract(Signal signal_a, Signal signal_b);
Signal amplitude_accumulate(Signal signal);
Signal amplitude_smooth(Signal signal, SmoothParams params);
Signal amplitude_clip_hard(Signal signal, ClipParams params);
Signal amplitude_clip_soft(Signal signal, ClipParams params, float curve);
Signal amplitude_normalize(Signal signal, NormalizeParams params);
Signal amplitude_divide(float numerator, Signal denominator);

Signal delay_unit(Signal signal);
Signal delay_line(Signal signal, DelayLineParams params);
Signal delay_fractional(Signal signal, FractionalDelayParams params);
Signal delay_tap_feedback(Buffer delay_line, TapParams params);
Signal delay_tap_feedforward(Buffer delay_line, TapParams params);

Signal filter_fir(Signal signal, float *kernel, uint32_t kernel_size);
Signal filter_biquad(Signal signal, BiquadParams params);
Signal filter_comb_ff(Signal signal, CombParams params);
Signal filter_comb_fb(Signal signal, CombParams params);
Signal filter_allpass(Signal signal, CombParams params);
Signal filter_dc_block(Signal signal, DcBlockParams params);
Signal filter_integrate(Signal signal);
Signal filter_differentiate(Signal signal);

Signal detect_peak(Signal signal, DetectorParams params);
Signal detect_rms(Signal signal, RmsParams params);
Signal detect_envelope(Signal signal, DetectorParams params);
Signal detect_threshold(Signal signal, ThresholdParams params);
Signal detect_zero_crossing(Signal signal);
Signal detect_slope(Signal signal);
Signal detect_autocorrelate(Signal signal, AutocorrelateParams params);

Signal modulation_ring(Signal signal, Buffer modulator);
Signal modulation_phase(Signal signal, ModulationParams params);
Signal modulation_frequency(Signal signal, ModulationParams params);
Signal modulation_amplitude(Signal signal, ModulationParams params);
Signal modulation_scrub(Buffer buffer, ScrubParams params);

float interpolation_linear(float sample_a, float sample_b, float t);
float interpolation_cubic(CubicInterpParams params);
float interpolation_sinc(SincInterpParams params);
float interpolation_lagrange(LagrangeInterpParams params);

Signal src_upsample(Signal signal, SrcParams params);
Signal src_downsample(Signal signal, SrcParams params);
Signal src_antialias(Signal signal, FilterParams params);
Signal src_antiimage(Signal signal, FilterParams params);
Signal src_convert_format(Signal signal, uint32_t from_format, uint32_t to_format);

Spectrum freq_fft(Signal signal, FftParams params);
Signal freq_ifft(Spectrum spectrum, uint32_t block_size);
Signal freq_window(Signal signal, WindowType window_type, uint32_t block_size);
Spectrum freq_multiply(FreqMultiplyParams params);
Signal freq_overlap_add(OverlapAddParams params);
Buffer *freq_overlap_save(OverlapSaveParams params);

Signal mix_crossfade(Signal signal_a, Signal signal_b, CrossfadeParams params);
Signal mix_wet_dry(Signal dry, Signal wet, WetDryParams params);
Buffer *mix_matrix(Buffer *signals, MixMatrixParams params);
Signal *mix_pan_stereo(Signal signal, PanParams params);
Signal *mix_encode_ms(Signal signal_l, Signal signal_r);
Signal *mix_decode_ms(Signal signal_m, Signal signal_s);

Signal nonlinear_waveshape(Signal signal, WaveshapeParams params);
Signal nonlinear_bitcrush(Signal signal, BitcrushParams params);
Signal nonlinear_samplerate_reduce(Signal signal, SampleRateReduceParams params);

#endif //DSP_ATOMS_H