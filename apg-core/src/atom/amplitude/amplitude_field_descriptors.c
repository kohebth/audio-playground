/* Generated from schema/atoms/atoms.json by codegen/generate_atom_artifacts.pl. Do not edit. */
#include "atom/atom_field_descriptors.h"
#include "atom/dsp_atoms.h"

#include <stddef.h>

#define FIELD_COUNT(name) const int name##_count = (int)(sizeof(name) / sizeof((name)[0]))

// clang-format off
const atom_field_desc_t amplitude_accumulate_state_fields[] = {
    {"accumulator", FIELD_FLOAT, offsetof(amplitude_accumulate_state_t, accumulator)},
};
FIELD_COUNT(amplitude_accumulate_state_fields);

const atom_field_desc_t amplitude_latch_config_fields[] = {
    {"threshold", FIELD_FLOAT, offsetof(amplitude_latch_params_t, threshold)},
};
FIELD_COUNT(amplitude_latch_config_fields);

const atom_field_desc_t amplitude_latch_state_fields[] = {
    {"latched_value", FIELD_FLOAT, offsetof(amplitude_latch_state_t, latched_value)},
    {"prev_gate", FIELD_INT, offsetof(amplitude_latch_state_t, prev_gate)},
};
FIELD_COUNT(amplitude_latch_state_fields);

const atom_field_desc_t amplitude_clip_hard_config_fields[] = {
    {"threshold", FIELD_FLOAT, offsetof(amplitude_clip_hard_params_t, threshold)},
};
FIELD_COUNT(amplitude_clip_hard_config_fields);

const atom_field_desc_t amplitude_clip_soft_config_fields[] = {
    {"threshold", FIELD_FLOAT, offsetof(amplitude_clip_soft_params_t, threshold)},
    {"curve", FIELD_INT, offsetof(amplitude_clip_soft_params_t, curve)},
};
FIELD_COUNT(amplitude_clip_soft_config_fields);

const atom_field_desc_t amplitude_divide_config_fields[] = {
    {"epsilon", FIELD_FLOAT, offsetof(amplitude_divide_params_t, epsilon)},
};
FIELD_COUNT(amplitude_divide_config_fields);

const atom_field_desc_t amplitude_gain_db_config_fields[] = {
    {"gain_db", FIELD_FLOAT, offsetof(amplitude_gain_db_params_t, gain_db)},
};
FIELD_COUNT(amplitude_gain_db_config_fields);

const atom_field_desc_t amplitude_normalize_config_fields[] = {
    {"target_level", FIELD_FLOAT, offsetof(amplitude_normalize_params_t, target_level)},
    {"mode", FIELD_INT, offsetof(amplitude_normalize_params_t, mode)},
};
FIELD_COUNT(amplitude_normalize_config_fields);

const atom_field_desc_t amplitude_normalize_state_fields[] = {
    {"running_peak", FIELD_FLOAT, offsetof(amplitude_normalize_state_t, running_peak)},
};
FIELD_COUNT(amplitude_normalize_state_fields);

const atom_field_desc_t amplitude_smooth_config_fields[] = {
    {"attack", FIELD_FLOAT, offsetof(amplitude_smooth_params_t, attack)},
    {"release", FIELD_FLOAT, offsetof(amplitude_smooth_params_t, release)},
};
FIELD_COUNT(amplitude_smooth_config_fields);

const atom_field_desc_t amplitude_smooth_state_fields[] = {
    {"prev_value", FIELD_FLOAT, offsetof(amplitude_smooth_state_t, prev_value)},
};
FIELD_COUNT(amplitude_smooth_state_fields);

// clang-format on

#undef FIELD_COUNT
