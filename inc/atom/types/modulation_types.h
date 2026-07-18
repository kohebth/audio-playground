/* Generated from schema/atoms/atoms.json by tools/generate_atom_artifacts.pl. Do not edit. */
#ifndef AUDIO_PLAYGROUND_MODULATION_TYPES_H
#define AUDIO_PLAYGROUND_MODULATION_TYPES_H

#include <atom/types/dsp_type_macros.h>

#define APG_MODULATION_DELAY_CAPACITY 4096u

// clang-format off
#define APG_MODULATION_DSP_TYPE_TABLE(X) \
    X(modulation_amplitude, SIGNAL, SIGNAL_MODULATOR, { float depth; }, { uint8_t _reserved; }) \
    X(modulation_frequency, SIGNAL, SIGNAL_MODULATOR, { float depth; }, { float *buffer; uint32_t buffer_len; int write_pos; float current_delay; }) \
    X(modulation_phase, SIGNAL, SIGNAL_MODULATOR, { float depth; }, { float *buffer; uint32_t buffer_len; int write_pos; }) \
    X(modulation_phaser, SIGNAL, SIGNAL_MODULATOR, { float center_frequency; float depth; float feedback; }, { float *buffer; uint32_t buffer_len; float feedback_sample; }) \
    X(modulation_ring, SIGNAL, SIGNAL_MODULATOR, { uint8_t _reserved; }, { uint8_t _reserved; }) \
    X(modulation_scrub, SIGNAL, BUFFER_POSITION, { int buffer_size; }, { uint8_t _reserved; })
// clang-format on

APG_MODULATION_DSP_TYPE_TABLE(APG_DECLARE_DSP_TYPES)

#endif // AUDIO_PLAYGROUND_MODULATION_TYPES_H
