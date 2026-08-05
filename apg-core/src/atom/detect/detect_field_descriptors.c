/* Generated from schema/atoms/atoms.json by codegen/generate_atom_artifacts.pl. Do not edit. */
#include "atom/atom_field_descriptors.h"
#include "atom/dsp_atoms.h"

#include <stddef.h>

#define FIELD_COUNT(name) const int name##_count = (int)(sizeof(name) / sizeof((name)[0]))

// clang-format off
const atom_field_desc_t detect_autocorrelate_config_fields[] = {
    {"max_lag", FIELD_INT, offsetof(detect_autocorrelate_params_t, max_lag)},
};
FIELD_COUNT(detect_autocorrelate_config_fields);

const atom_field_desc_t detect_autocorrelate_state_fields[] = {
    {"buffer", FIELD_BUFFER, offsetof(detect_autocorrelate_state_t, buffer), APG_DETECT_AUTOCORRELATION_CAPACITY},
    {"buffer_len", FIELD_INT, offsetof(detect_autocorrelate_state_t, buffer_len)},
    {"write_pos", FIELD_INT, offsetof(detect_autocorrelate_state_t, write_pos)},
};
FIELD_COUNT(detect_autocorrelate_state_fields);

const atom_field_desc_t detect_pitch_config_fields[] = {
    {"max_lag", FIELD_INT, offsetof(detect_pitch_params_t, max_lag)},
};
FIELD_COUNT(detect_pitch_config_fields);

const atom_field_desc_t detect_pitch_state_fields[] = {
    {"buffer", FIELD_BUFFER, offsetof(detect_pitch_state_t, buffer), APG_DETECT_AUTOCORRELATION_CAPACITY},
    {"buffer_len", FIELD_INT, offsetof(detect_pitch_state_t, buffer_len)},
    {"write_pos", FIELD_INT, offsetof(detect_pitch_state_t, write_pos)},
};
FIELD_COUNT(detect_pitch_state_fields);

const atom_field_desc_t detect_envelope_config_fields[] = {
    {"attack", FIELD_FLOAT, offsetof(detect_envelope_params_t, attack)},
    {"release", FIELD_FLOAT, offsetof(detect_envelope_params_t, release)},
};
FIELD_COUNT(detect_envelope_config_fields);

const atom_field_desc_t detect_envelope_state_fields[] = {
    {"prev_envelope", FIELD_FLOAT, offsetof(detect_envelope_state_t, prev_envelope)},
};
FIELD_COUNT(detect_envelope_state_fields);

const atom_field_desc_t detect_peak_config_fields[] = {
    {"attack", FIELD_FLOAT, offsetof(detect_peak_params_t, attack)},
    {"release", FIELD_FLOAT, offsetof(detect_peak_params_t, release)},
};
FIELD_COUNT(detect_peak_config_fields);

const atom_field_desc_t detect_peak_state_fields[] = {
    {"prev_peak", FIELD_FLOAT, offsetof(detect_peak_state_t, prev_peak)},
};
FIELD_COUNT(detect_peak_state_fields);

const atom_field_desc_t detect_rms_config_fields[] = {
    {"window_size", FIELD_INT, offsetof(detect_rms_params_t, window_size)},
};
FIELD_COUNT(detect_rms_config_fields);

const atom_field_desc_t detect_rms_state_fields[] = {
    {"buffer", FIELD_BUFFER, offsetof(detect_rms_state_t, buffer), APG_DETECT_RMS_CAPACITY},
    {"buffer_len", FIELD_INT, offsetof(detect_rms_state_t, buffer_len)},
    {"write_pos", FIELD_INT, offsetof(detect_rms_state_t, write_pos)},
    {"sum", FIELD_FLOAT, offsetof(detect_rms_state_t, sum)},
};
FIELD_COUNT(detect_rms_state_fields);

const atom_field_desc_t detect_slope_state_fields[] = {
    {"prev_sample", FIELD_FLOAT, offsetof(detect_slope_state_t, prev_sample)},
};
FIELD_COUNT(detect_slope_state_fields);

const atom_field_desc_t detect_threshold_config_fields[] = {
    {"threshold", FIELD_FLOAT, offsetof(detect_threshold_params_t, threshold)},
};
FIELD_COUNT(detect_threshold_config_fields);

const atom_field_desc_t detect_zero_crossing_state_fields[] = {
    {"prev_sample", FIELD_FLOAT, offsetof(detect_zero_crossing_state_t, prev_sample)},
};
FIELD_COUNT(detect_zero_crossing_state_fields);

// clang-format on

#undef FIELD_COUNT
