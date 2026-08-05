#ifndef AUDIO_PLAYGROUND_DSP_TYPE_CHECKS_H
#define AUDIO_PLAYGROUND_DSP_TYPE_CHECKS_H

#include <atom/atom_definitions.h>
#include <atom/types/amplitude_types.h>
#include <atom/types/delay_types.h>
#include <atom/types/detect_types.h>
#include <atom/types/filter_types.h>
#include <atom/types/frequency_types.h>
#include <atom/types/generation_types.h>
#include <atom/types/interpolation_types.h>
#include <atom/types/math_types.h>
#include <atom/types/mix_types.h>
#include <atom/types/modulation_types.h>
#include <atom/types/nonlinear_types.h>
#include <atom/types/src_types.h>

#define APG_ALL_DSP_TYPE_TABLES(X)      \
    APG_AMPLITUDE_DSP_TYPE_TABLE(X)     \
    APG_DELAY_DSP_TYPE_TABLE(X)         \
    APG_DETECT_DSP_TYPE_TABLE(X)        \
    APG_FILTER_DSP_TYPE_TABLE(X)        \
    APG_FREQUENCY_DSP_TYPE_TABLE(X)     \
    APG_GENERATION_DSP_TYPE_TABLE(X)    \
    APG_INTERPOLATION_DSP_TYPE_TABLE(X) \
    APG_MATH_DSP_TYPE_TABLE(X)          \
    APG_MIX_DSP_TYPE_TABLE(X)           \
    APG_MODULATION_DSP_TYPE_TABLE(X)    \
    APG_NONLINEAR_DSP_TYPE_TABLE(X)     \
    APG_SRC_DSP_TYPE_TABLE(X)

#define APG_DECLARE_DSP_TYPE_MARKER(atom_name, out_profile, in_profile, params_fields, state_fields) \
    enum { APG_DSP_TYPE_PRESENT_##atom_name = 1 };
APG_ALL_DSP_TYPE_TABLES(APG_DECLARE_DSP_TYPE_MARKER)

#define APG_COUNT_DSP_TYPE_ROW(atom_name, out_profile, in_profile, params_fields, state_fields) +1
#define APG_COUNT_ATOM_DEFINITION(                                                         \
    atom_name, category, input_count, config_count, state_count, flags, maturity, dispatch \
)                                                                                          \
    +1

#if defined(__cplusplus)
#define APG_DSP_STATIC_ASSERT(condition, message) static_assert(condition, message)
#else
#define APG_DSP_STATIC_ASSERT(condition, message) _Static_assert(condition, message)
#endif

APG_DSP_STATIC_ASSERT(
    (0 APG_ALL_DSP_TYPE_TABLES(APG_COUNT_DSP_TYPE_ROW)) == (0 APG_ATOM_DEFINITIONS(APG_COUNT_ATOM_DEFINITION)),
    "DSP type tables must contain exactly one row per canonical atom"
);

#define APG_CHECK_DSP_TYPE_PRESENT(                                                        \
    atom_name, category, input_count, config_count, state_count, flags, maturity, dispatch \
)                                                                                          \
    APG_DSP_STATIC_ASSERT(APG_DSP_TYPE_PRESENT_##atom_name == 1, "canonical atom is missing its DSP types");
APG_ATOM_DEFINITIONS(APG_CHECK_DSP_TYPE_PRESENT)

#undef APG_CHECK_DSP_TYPE_PRESENT
#undef APG_DSP_STATIC_ASSERT
#undef APG_COUNT_ATOM_DEFINITION
#undef APG_COUNT_DSP_TYPE_ROW
#undef APG_DECLARE_DSP_TYPE_MARKER
#undef APG_ALL_DSP_TYPE_TABLES

#undef APG_SRC_DSP_TYPE_TABLE
#undef APG_NONLINEAR_DSP_TYPE_TABLE
#undef APG_MODULATION_DSP_TYPE_TABLE
#undef APG_MIX_DSP_TYPE_TABLE
#undef APG_MATH_DSP_TYPE_TABLE
#undef APG_INTERPOLATION_DSP_TYPE_TABLE
#undef APG_GENERATION_DSP_TYPE_TABLE
#undef APG_FREQUENCY_DSP_TYPE_TABLE
#undef APG_FILTER_DSP_TYPE_TABLE
#undef APG_DETECT_DSP_TYPE_TABLE
#undef APG_DELAY_DSP_TYPE_TABLE
#undef APG_AMPLITUDE_DSP_TYPE_TABLE

#undef APG_DECLARE_DSP_TYPES
#undef APG_EXPAND_IO_FIELDS_INNER
#undef APG_EXPAND_IO_FIELDS
#undef APG_IO_FIELDS_SIGNAL_MODULATOR
#undef APG_IO_FIELDS_SIGNAL_GATE
#undef APG_IO_FIELDS_SIGNAL_DELAY
#undef APG_IO_FIELDS_SIGNAL_CUTOFF
#undef APG_IO_FIELDS_SIGNAL_MATRIX
#undef APG_IO_FIELDS_BUFFER_POSITION
#undef APG_IO_FIELDS_INTERPOLATION_LINEAR
#undef APG_IO_FIELDS_SAMPLES_T
#undef APG_IO_FIELDS_INTERPOLATION_CUBIC
#undef APG_IO_FIELDS_FREQUENCY
#undef APG_IO_FIELDS_SIGNAL_PITCH_SHIFT
#undef APG_IO_FIELDS_FRAME
#undef APG_IO_FIELDS_COMPLEX_PAIR
#undef APG_IO_FIELDS_BUFFER_TAP
#undef APG_IO_FIELDS_TRIGGER
#undef APG_IO_FIELDS_GATE
#undef APG_IO_FIELDS_SLOPE
#undef APG_IO_FIELDS_LEVEL
#undef APG_IO_FIELDS_ENVELOPE
#undef APG_IO_FIELDS_PITCH
#undef APG_IO_FIELDS_CORRELATION
#undef APG_IO_FIELDS_DIVISION
#undef APG_IO_FIELDS_WET_DRY
#undef APG_IO_FIELDS_MS
#undef APG_IO_FIELDS_COMPLEX
#undef APG_IO_FIELDS_STEREO
#undef APG_IO_FIELDS_SIGNAL_PAIR
#undef APG_IO_FIELDS_SIGNAL
#undef APG_IO_FIELDS_EMPTY

#endif // AUDIO_PLAYGROUND_DSP_TYPE_CHECKS_H
