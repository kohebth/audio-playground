#ifndef AUDIO_PLAYGROUND_SRC_TYPES_H
#define AUDIO_PLAYGROUND_SRC_TYPES_H

#include <atom/types/dsp_type_macros.h>

// clang-format off
#define APG_SRC_DSP_TYPE_TABLE(X) \
    X(src_antialias, SIGNAL, SIGNAL, { float cutoff; float sample_rate; }, { float z1; float z2; }) \
    X(src_antiimage, SIGNAL, SIGNAL, { float cutoff; float sample_rate; }, { float z1; float z2; }) \
    X(src_convert_format, SIGNAL, SIGNAL, { int from_format; int to_format; }, { uint8_t _reserved; }) \
    X(src_downsample, SIGNAL, SIGNAL, { int factor; }, { uint8_t _reserved; }) \
    X(src_upsample, SIGNAL, SIGNAL, { int factor; }, { uint8_t _reserved; })
// clang-format on

APG_SRC_DSP_TYPE_TABLE(APG_DECLARE_DSP_TYPES)

#endif // AUDIO_PLAYGROUND_SRC_TYPES_H
