/* Generated from schema/atoms/atoms.json by codegen/generate_atom_artifacts.pl. Do not edit. */
#ifndef AUDIO_PLAYGROUND_NONLINEAR_TYPES_H
#define AUDIO_PLAYGROUND_NONLINEAR_TYPES_H

#include <atom/types/dsp_type_macros.h>

// clang-format off
#define APG_NONLINEAR_DSP_TYPE_TABLE(X) \
    X(nonlinear_bitcrush, SIGNAL, SIGNAL, { float bit_depth; }, { uint8_t _reserved; }) \
    X(nonlinear_sample_hold, SIGNAL, SIGNAL, { float factor; }, { float last_val; float counter; }) \
    X(nonlinear_waveshape, SIGNAL, SIGNAL, { float *transfer_table; int table_size; }, { uint8_t _reserved; })
// clang-format on

APG_NONLINEAR_DSP_TYPE_TABLE(APG_DECLARE_DSP_TYPES)

#endif // AUDIO_PLAYGROUND_NONLINEAR_TYPES_H
