#ifndef AUDIO_PLAYGROUND_FILTER_TYPES_H
#define AUDIO_PLAYGROUND_FILTER_TYPES_H

#include <atom/types/dsp_type_macros.h>

// clang-format off
#define APG_FILTER_DSP_TYPE_TABLE(X) \
    X(filter_allpass, SIGNAL, SIGNAL, { int delay_samples; float coefficient; }, { float *buffer; uint32_t buffer_len; int write_pos; }) \
    X(filter_biquad_coefficients, SIGNAL, SIGNAL, { float b0; float b1; float b2; float a1; float a2; }, { float z1; float z2; }) \
    X(filter_biquad, SIGNAL, SIGNAL_CUTOFF, { float cutoff; float q; int mode; float smoothing_ms; }, { float z1; float z2; float current_cutoff; float current_q; float current_b0; float current_b1; float current_b2; float current_a1; float current_a2; }) \
    X(filter_comb_fb, SIGNAL, SIGNAL_DELAY, { int delay_samples; float coefficient; }, { float *buffer; uint32_t buffer_len; int write_pos; }) \
    X(filter_comb_ff, SIGNAL, SIGNAL, { int delay_samples; float coefficient; }, { float *buffer; uint32_t buffer_len; int write_pos; }) \
    X(filter_dc_block, SIGNAL, SIGNAL, { float coefficient; }, { float prev_input; float prev_output; }) \
    X(filter_differentiate, SIGNAL, SIGNAL, { uint8_t _reserved; }, { float prev_sample; }) \
    X(filter_fir, SIGNAL, SIGNAL, { float *kernel; int kernel_size; }, { float *buffer; uint32_t buffer_len; int write_pos; }) \
    X(filter_integrate, SIGNAL, SIGNAL, { uint8_t _reserved; }, { float accumulator; })
// clang-format on

APG_FILTER_DSP_TYPE_TABLE(APG_DECLARE_DSP_TYPES)

#endif // AUDIO_PLAYGROUND_FILTER_TYPES_H
