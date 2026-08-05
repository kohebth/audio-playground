/* Generated from schema/atoms/atoms.json by codegen/generate_atom_artifacts.pl. Do not edit. */
#ifndef AUDIO_PLAYGROUND_DSP_TYPE_MACROS_H
#define AUDIO_PLAYGROUND_DSP_TYPE_MACROS_H

#include <stdint.h>

/* I/O profiles describe C member layouts, not YAML binding contracts. */
// clang-format off
#define APG_IO_FIELDS_EMPTY                  { uint8_t _reserved; }
#define APG_IO_FIELDS_SIGNAL                 { float *signal; }
#define APG_IO_FIELDS_SIGNAL_PAIR            { float *signal_a; float *signal_b; }
#define APG_IO_FIELDS_STEREO                 { float *left; float *right; }
#define APG_IO_FIELDS_COMPLEX                { float *real; float *imag; }
#define APG_IO_FIELDS_MS                     { float *mid; float *side; }
#define APG_IO_FIELDS_WET_DRY                { float *dry; float *wet; }
#define APG_IO_FIELDS_DIVISION               { float *numerator; float *denominator; }
#define APG_IO_FIELDS_CORRELATION            { float *correlation; }
#define APG_IO_FIELDS_PITCH                  { float *pitch; }
#define APG_IO_FIELDS_ENVELOPE               { float *envelope; }
#define APG_IO_FIELDS_LEVEL                  { float *level; }
#define APG_IO_FIELDS_SLOPE                  { float *slope; }
#define APG_IO_FIELDS_GATE                   { float *gate; }
#define APG_IO_FIELDS_TRIGGER                { float *trigger; }
#define APG_IO_FIELDS_BUFFER_TAP             { float *buffer; int tap_position; }
#define APG_IO_FIELDS_COMPLEX_PAIR           { float *real_a; float *imag_a; float *real_b; float *imag_b; }
#define APG_IO_FIELDS_FRAME                  { float *frame; }
#define APG_IO_FIELDS_SIGNAL_PITCH_SHIFT     { float *signal; float *pitch_shift; }
#define APG_IO_FIELDS_FREQUENCY              { float *frequency; }
#define APG_IO_FIELDS_INTERPOLATION_CUBIC    { float *signal_n1; float *signal_a; float *signal_b; float *signal_c; float *t; }
#define APG_IO_FIELDS_SAMPLES_T              { float *samples; float *t; }
#define APG_IO_FIELDS_INTERPOLATION_LINEAR   { float *signal_a; float *signal_b; float *t; }
#define APG_IO_FIELDS_BUFFER_POSITION        { float *buffer; float *position; }
#define APG_IO_FIELDS_SIGNAL_MATRIX          { float **signals; }
#define APG_IO_FIELDS_SIGNAL_CUTOFF          { float *signal; float *cutoff; }
#define APG_IO_FIELDS_SIGNAL_DELAY           { float *signal; float *delay; }
#define APG_IO_FIELDS_SIGNAL_GATE            { float *signal; float *gate; }
#define APG_IO_FIELDS_SIGNAL_MODULATOR       { float *signal; float *modulator; }
// clang-format on

#define APG_EXPAND_IO_FIELDS(profile)       APG_EXPAND_IO_FIELDS_INNER(profile)
#define APG_EXPAND_IO_FIELDS_INNER(profile) APG_IO_FIELDS_##profile

#define APG_DECLARE_DSP_TYPES(atom_name, out_profile, in_profile, params_fields, state_fields) \
    typedef struct APG_EXPAND_IO_FIELDS(out_profile) atom_name##_out_t;                        \
    typedef struct APG_EXPAND_IO_FIELDS(in_profile) atom_name##_in_t;                          \
    typedef struct params_fields atom_name##_params_t;                                         \
    typedef struct state_fields  atom_name##_state_t;

#endif // AUDIO_PLAYGROUND_DSP_TYPE_MACROS_H
