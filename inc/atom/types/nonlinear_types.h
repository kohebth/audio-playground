#ifndef AUDIO_PLAYGROUND_NONLINEAR_TYPES_H
#define AUDIO_PLAYGROUND_NONLINEAR_TYPES_H

#include <atom/types/dsp_type_macros.h>

// clang-format off
#define APG_NONLINEAR_DSP_TYPE_TABLE(X) \
    X(nonlinear_bitcrush, SIGNAL, SIGNAL, { float bit_depth; }, {}) \
    X(nonlinear_sample_hold, SIGNAL, SIGNAL, { float factor; }, { float last_val; float counter; }) \
    X(nonlinear_waveshape, SIGNAL, SIGNAL, { float *transfer_table; int table_size; }, {})
// clang-format on

APG_NONLINEAR_DSP_TYPE_TABLE(APG_DECLARE_DSP_TYPES)

#endif // AUDIO_PLAYGROUND_NONLINEAR_TYPES_H
