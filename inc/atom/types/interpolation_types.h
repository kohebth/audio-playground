/* Generated from schema/atoms/atoms.json by tools/generate_atom_artifacts.pl. Do not edit. */
#ifndef AUDIO_PLAYGROUND_INTERPOLATION_TYPES_H
#define AUDIO_PLAYGROUND_INTERPOLATION_TYPES_H

#include <atom/types/dsp_type_macros.h>

// clang-format off
#define APG_INTERPOLATION_DSP_TYPE_TABLE(X) \
    X(interpolation_cubic, SIGNAL, INTERPOLATION_CUBIC, { uint8_t _reserved; }, { uint8_t _reserved; }) \
    X(interpolation_lagrange, SIGNAL, SAMPLES_T, { int order; }, { uint8_t _reserved; }) \
    X(interpolation_linear, SIGNAL, INTERPOLATION_LINEAR, { uint8_t _reserved; }, { uint8_t _reserved; }) \
    X(interpolation_sinc, SIGNAL, BUFFER_POSITION, { int num_taps; }, { uint8_t _reserved; })
// clang-format on

APG_INTERPOLATION_DSP_TYPE_TABLE(APG_DECLARE_DSP_TYPES)

#endif // AUDIO_PLAYGROUND_INTERPOLATION_TYPES_H
