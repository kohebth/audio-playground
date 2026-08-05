/* Generated from schema/atoms/atoms.json by codegen/generate_atom_artifacts.pl. Do not edit. */
#ifndef AUDIO_PLAYGROUND_MATH_TYPES_H
#define AUDIO_PLAYGROUND_MATH_TYPES_H

#include <atom/types/dsp_type_macros.h>

// clang-format off
#define APG_MATH_DSP_TYPE_TABLE(X) \
    X(math_difference, SIGNAL, SIGNAL, { uint8_t _reserved; }, { float prev_sample; }) \
    X(math_integrate, SIGNAL, SIGNAL, { float leakage; }, { float accumulator; })
// clang-format on

APG_MATH_DSP_TYPE_TABLE(APG_DECLARE_DSP_TYPES)

#endif // AUDIO_PLAYGROUND_MATH_TYPES_H
