#include "atom/atom_field_descriptors.h"
#include "atom/dsp_atoms.h"

#include <stddef.h>

#define FIELD_COUNT(name) const int name##_count = (int)(sizeof(name) / sizeof((name)[0]))

const atom_field_desc_t delay_fractional_config_fields[] = {
    {"delay_samples", FIELD_FLOAT, offsetof(delay_fractional_params_t, delay_samples)},
    {"interpolation",   FIELD_INT, offsetof(delay_fractional_params_t, interpolation)},
};
FIELD_COUNT(delay_fractional_config_fields);

const atom_field_desc_t delay_fractional_state_fields[] = {
    {"buffer", FIELD_BUFFER, offsetof(delay_fractional_state_t, buffer), 192000u},
    {"write_pos", FIELD_INT, offsetof(delay_fractional_state_t, write_pos)},
};
FIELD_COUNT(delay_fractional_state_fields);

const atom_field_desc_t delay_line_config_fields[] = {
    {"length", FIELD_INT, offsetof(delay_line_params_t, length)},
};
FIELD_COUNT(delay_line_config_fields);

const atom_field_desc_t delay_line_state_fields[] = {
    {"buffer", FIELD_BUFFER, offsetof(delay_line_state_t, buffer), 192000u},
    {"write_pos", FIELD_INT, offsetof(delay_line_state_t, write_pos)},
};
FIELD_COUNT(delay_line_state_fields);

const atom_field_desc_t delay_tap_feedback_config_fields[] = {
    {"coefficient", FIELD_FLOAT, offsetof(delay_tap_feedback_params_t, coefficient)},
};
FIELD_COUNT(delay_tap_feedback_config_fields);

const atom_field_desc_t delay_tap_feedforward_config_fields[] = {
    {"coefficient", FIELD_FLOAT, offsetof(delay_tap_feedforward_params_t, coefficient)},
};
FIELD_COUNT(delay_tap_feedforward_config_fields);

const atom_field_desc_t delay_tap_feedback_in_fields[] = {
    {      "buffer", FIELD_SIGNAL, offsetof(delay_tap_feedback_in_t,       buffer)},
    {"tap_position",    FIELD_INT, offsetof(delay_tap_feedback_in_t, tap_position)},
};
FIELD_COUNT(delay_tap_feedback_in_fields);

const atom_field_desc_t delay_tap_feedforward_in_fields[] = {
    {      "buffer", FIELD_SIGNAL, offsetof(delay_tap_feedforward_in_t,       buffer)},
    {"tap_position",    FIELD_INT, offsetof(delay_tap_feedforward_in_t, tap_position)},
};
FIELD_COUNT(delay_tap_feedforward_in_fields);

const atom_field_desc_t delay_unit_state_fields[] = {
    {"prev_sample", FIELD_FLOAT, offsetof(delay_unit_state_t, prev_sample)},
};
FIELD_COUNT(delay_unit_state_fields);

#undef FIELD_COUNT
