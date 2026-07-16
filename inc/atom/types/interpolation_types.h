#ifndef AUDIO_PLAYGROUND_INTERPOLATION_TYPES_H
#define AUDIO_PLAYGROUND_INTERPOLATION_TYPES_H

#include <atom/types/dsp_type_macros.h>

// clang-format off
#define APG_INTERPOLATION_DSP_TYPE_TABLE(X) \
    X(interpolation_cubic, SIGNAL, INTERPOLATION_CUBIC, { uint8_t _reserved; }, { uint8_t _reserved; }) \
    X(interpolation_lagrange, SIGNAL, SAMPLES_T, { int order; }, { float *buffer; int write_pos; }) \
    X(interpolation_linear, SIGNAL, INTERPOLATION_LINEAR, { uint8_t _reserved; }, { uint8_t _reserved; }) \
    X(interpolation_sinc, SIGNAL, BUFFER_POSITION, { int num_taps; }, { float *taps; })
// clang-format on

APG_INTERPOLATION_DSP_TYPE_TABLE(APG_DECLARE_DSP_TYPES)

#endif // AUDIO_PLAYGROUND_INTERPOLATION_TYPES_H
