#ifndef AUDIO_PLAYGROUND_AMPLITUDE_TYPES_H
#define AUDIO_PLAYGROUND_AMPLITUDE_TYPES_H

#include <atom/types/dsp_type_macros.h>

// clang-format off
#define APG_AMPLITUDE_DSP_TYPE_TABLE(X) \
    X(amplitude_accumulate, SIGNAL, SIGNAL, {}, { float accumulator; }) \
    X(amplitude_latch, SIGNAL, SIGNAL_GATE, { float threshold; }, { float latched_value; int prev_gate; }) \
    X(amplitude_add, SIGNAL, SIGNAL_PAIR, {}, {}) \
    X(amplitude_clip_hard, SIGNAL, SIGNAL, { float threshold; }, {}) \
    X(amplitude_clip_soft, SIGNAL, SIGNAL, { float threshold; int curve; }, {}) \
    X(amplitude_divide, SIGNAL, DIVISION, { float epsilon; }, {}) \
    X(amplitude_multiply, SIGNAL, SIGNAL_PAIR, {}, {}) \
    X(amplitude_normalize, SIGNAL, SIGNAL, { float target_level; int mode; }, { float running_peak; }) \
    X(amplitude_smooth, SIGNAL, SIGNAL, { float attack; float release; float sample_rate; }, { float prev_value; }) \
    X(amplitude_subtract, SIGNAL, SIGNAL_PAIR, {}, {})
// clang-format on

APG_AMPLITUDE_DSP_TYPE_TABLE(APG_DECLARE_DSP_TYPES)

#endif // AUDIO_PLAYGROUND_AMPLITUDE_TYPES_H
