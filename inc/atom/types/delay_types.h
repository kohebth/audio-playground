#ifndef AUDIO_PLAYGROUND_DELAY_TYPES_H
#define AUDIO_PLAYGROUND_DELAY_TYPES_H

#include <atom/types/dsp_type_macros.h>

// clang-format off
#define APG_DELAY_DSP_TYPE_TABLE(X) \
    X(delay_fractional, SIGNAL, SIGNAL, { float delay_samples; int interpolation; }, { float *buffer; uint32_t buffer_len; int write_pos; }) \
    X(delay_line, SIGNAL, SIGNAL, { int length; }, { float *buffer; uint32_t buffer_len; int write_pos; }) \
    X(delay_tap_feedback, SIGNAL, BUFFER_TAP, { float coefficient; }, { uint8_t _reserved; }) \
    X(delay_tap_feedforward, SIGNAL, BUFFER_TAP, { float coefficient; }, { uint8_t _reserved; }) \
    X(delay_unit, SIGNAL, SIGNAL, { uint8_t _reserved; }, { float prev_sample; })
// clang-format on

APG_DELAY_DSP_TYPE_TABLE(APG_DECLARE_DSP_TYPES)

#endif // AUDIO_PLAYGROUND_DELAY_TYPES_H
