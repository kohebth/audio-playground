/* Generated from schema/atoms/atoms.json by tools/generate_atom_artifacts.pl. Do not edit. */
#ifndef AUDIO_PLAYGROUND_MIX_TYPES_H
#define AUDIO_PLAYGROUND_MIX_TYPES_H

#include <atom/types/dsp_type_macros.h>

// clang-format off
#define APG_MIX_DSP_TYPE_TABLE(X) \
    X(mix_crossfade, SIGNAL, SIGNAL_PAIR, { float t; int curve; }, { uint8_t _reserved; }) \
    X(mix_decode_ms, STEREO, MS, { uint8_t _reserved; }, { uint8_t _reserved; }) \
    X(mix_encode_ms, MS, STEREO, { uint8_t _reserved; }, { uint8_t _reserved; }) \
    X(mix_matrix, SIGNAL_MATRIX, SIGNAL_MATRIX, { float **coefficients; int num_in; int num_out; }, { uint8_t _reserved; }) \
    X(mix_pan_stereo, STEREO, SIGNAL, { float position; }, { uint8_t _reserved; }) \
    X(mix_wet_dry, SIGNAL, WET_DRY, { float mix; }, { uint8_t _reserved; })
// clang-format on

APG_MIX_DSP_TYPE_TABLE(APG_DECLARE_DSP_TYPES)

#endif // AUDIO_PLAYGROUND_MIX_TYPES_H
