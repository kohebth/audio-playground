#ifndef AUDIO_PLAYGROUND_AMPLITUDE_TYPES_H
#define AUDIO_PLAYGROUND_AMPLITUDE_TYPES_H

#include <atom/types/dsp_type_macros.h>

// clang-format off
#define APG_AMPLITUDE_DSP_TYPE_TABLE(X) \
    X(amplitude_accumulate, SIGNAL, SIGNAL, { uint8_t _reserved; }, { float accumulator; }) \
    X(amplitude_latch, SIGNAL, SIGNAL_GATE, { float threshold; }, { float latched_value; int prev_gate; }) \
    X(amplitude_add, SIGNAL, SIGNAL_PAIR, { uint8_t _reserved; }, { uint8_t _reserved; }) \
    X(amplitude_clip_hard, SIGNAL, SIGNAL, { float threshold; }, { uint8_t _reserved; }) \
    X(amplitude_clip_soft, SIGNAL, SIGNAL, { float threshold; int curve; }, { uint8_t _reserved; }) \
    X(amplitude_divide, SIGNAL, DIVISION, { float epsilon; }, { uint8_t _reserved; }) \
    X(amplitude_multiply, SIGNAL, SIGNAL_PAIR, { uint8_t _reserved; }, { uint8_t _reserved; }) \
    X(amplitude_normalize, SIGNAL, SIGNAL, { float target_level; int mode; }, { float running_peak; }) \
    X(amplitude_smooth, SIGNAL, SIGNAL, { float attack; float release; }, { float prev_value; }) \
    X(amplitude_subtract, SIGNAL, SIGNAL_PAIR, { uint8_t _reserved; }, { uint8_t _reserved; })
// clang-format on

APG_AMPLITUDE_DSP_TYPE_TABLE(APG_DECLARE_DSP_TYPES)

#endif // AUDIO_PLAYGROUND_AMPLITUDE_TYPES_H
