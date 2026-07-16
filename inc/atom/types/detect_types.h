/* Generated from schema/atoms/atoms.json by tools/generate_atom_artifacts.pl. Do not edit. */
#ifndef AUDIO_PLAYGROUND_DETECT_TYPES_H
#define AUDIO_PLAYGROUND_DETECT_TYPES_H

#include <atom/types/dsp_type_macros.h>

#define APG_DETECT_RMS_CAPACITY             4096u
#define APG_DETECT_AUTOCORRELATION_CAPACITY 1024u

// clang-format off
#define APG_DETECT_DSP_TYPE_TABLE(X) \
    X(detect_autocorrelate, CORRELATION, SIGNAL, { int max_lag; }, { float *buffer; uint32_t buffer_len; int write_pos; }) \
    X(detect_pitch, PITCH, SIGNAL, { int max_lag; }, { float *buffer; uint32_t buffer_len; int write_pos; }) \
    X(detect_envelope, ENVELOPE, SIGNAL, { float attack; float release; }, { float prev_envelope; }) \
    X(detect_peak, LEVEL, SIGNAL, { float attack; float release; }, { float prev_peak; }) \
    X(detect_rms, LEVEL, SIGNAL, { int window_size; }, { float *buffer; uint32_t buffer_len; int write_pos; float sum; }) \
    X(detect_slope, SLOPE, SIGNAL, { uint8_t _reserved; }, { float prev_sample; }) \
    X(detect_threshold, GATE, SIGNAL, { float threshold; }, { uint8_t _reserved; }) \
    X(detect_zero_crossing, TRIGGER, SIGNAL, { uint8_t _reserved; }, { float prev_sample; })
// clang-format on

APG_DETECT_DSP_TYPE_TABLE(APG_DECLARE_DSP_TYPES)

#endif // AUDIO_PLAYGROUND_DETECT_TYPES_H
