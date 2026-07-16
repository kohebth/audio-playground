#ifndef AUDIO_PLAYGROUND_DSP_ENUMS_H
#define AUDIO_PLAYGROUND_DSP_ENUMS_H

typedef enum apg_waveform_type {
    WAVEFORM_SINE        = 0,
    WAVEFORM_SAW         = 1,
    WAVEFORM_SQUARE      = 2,
    WAVEFORM_TRIANGLE    = 3,
    WAVEFORM_NOISE_WHITE = 4,
    WAVEFORM_NOISE_PINK  = 5,
    WAVEFORM_NOISE_BROWN = 6,
} apg_waveform_type_t;
typedef apg_waveform_type_t WaveformType;

typedef enum apg_normalize_mode {
    NORMALIZE_PEAK = 0,
    NORMALIZE_RMS  = 1,
} apg_normalize_mode_t;
typedef apg_normalize_mode_t NormalizeMode;

typedef enum apg_interpolation_type {
    INTERPOLATION_LINEAR   = 0,
    INTERPOLATION_CUBIC    = 1,
    INTERPOLATION_SINC     = 2,
    INTERPOLATION_LAGRANGE = 3,
} apg_interpolation_type_t;
typedef apg_interpolation_type_t InterpolationType;

typedef enum apg_window_type {
    WINDOW_HANN        = 0,
    WINDOW_HAMMING     = 1,
    WINDOW_BLACKMAN    = 2,
    WINDOW_RECTANGULAR = 3,
} apg_window_type_t;
typedef apg_window_type_t WindowType;

#endif // AUDIO_PLAYGROUND_DSP_ENUMS_H
