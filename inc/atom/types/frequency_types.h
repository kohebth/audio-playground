/* Generated from schema/atoms/atoms.json by tools/generate_atom_artifacts.pl. Do not edit. */
#ifndef AUDIO_PLAYGROUND_FREQUENCY_TYPES_H
#define AUDIO_PLAYGROUND_FREQUENCY_TYPES_H

#include <atom/types/dsp_type_macros.h>

// clang-format off
#define APG_FREQUENCY_DSP_TYPE_TABLE(X) \
    X(freq_fft, COMPLEX, SIGNAL, { int block_size; }, { float *workspace; uint32_t buffer_len; }) \
    X(freq_ifft, SIGNAL, COMPLEX, { int block_size; }, { float *workspace; uint32_t buffer_len; }) \
    X(freq_multiply, COMPLEX, COMPLEX_PAIR, { int block_size; }, { uint8_t _reserved; }) \
    X(freq_overlap_add, SIGNAL, FRAME, { int block_size; int hop_size; }, { float *buffer; uint32_t buffer_len; }) \
    X(freq_overlap_save, FRAME, SIGNAL, { int block_size; int hop_size; }, { float *buffer; uint32_t buffer_len; int write_pos; }) \
    X(freq_window, SIGNAL, SIGNAL, { int window_type; int block_size; }, { uint8_t _reserved; }) \
    X(freq_shift, SIGNAL, SIGNAL_PITCH_SHIFT, { int block_size; }, { float *buffer; uint32_t buffer_len; int write_pos; float read_ptr; }) \
    X(freq_quantize, SIGNAL, SIGNAL, { float unused; }, { float unused; })
// clang-format on

APG_FREQUENCY_DSP_TYPE_TABLE(APG_DECLARE_DSP_TYPES)

#endif // AUDIO_PLAYGROUND_FREQUENCY_TYPES_H
