#ifndef AUDIO_PLAYGROUND_MIX_TYPES_H
#define AUDIO_PLAYGROUND_MIX_TYPES_H

#include <atom/types/dsp_type_macros.h>

// clang-format off
#define APG_MIX_DSP_TYPE_TABLE(X) \
    X(mix_crossfade, SIGNAL, SIGNAL_PAIR, { float t; }, {}) \
    X(mix_decode_ms, STEREO, MS, {}, {}) \
    X(mix_encode_ms, MS, STEREO, {}, {}) \
    X(mix_matrix, SIGNAL_MATRIX, SIGNAL_MATRIX, { float **coefficients; int num_in; int num_out; }, {}) \
    X(mix_pan_stereo, STEREO, SIGNAL, { float position; }, {}) \
    X(mix_wet_dry, SIGNAL, WET_DRY, { float mix; }, {})
// clang-format on

APG_MIX_DSP_TYPE_TABLE(APG_DECLARE_DSP_TYPES)

#endif // AUDIO_PLAYGROUND_MIX_TYPES_H
