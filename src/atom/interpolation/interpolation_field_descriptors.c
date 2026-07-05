#include "atom/atom_field_descriptors.h"
#include "atom/dsp_atoms.h"

#include <stddef.h>

#define FIELD_COUNT(name) const int name##_count = (int)(sizeof(name) / sizeof((name)[0]))

const atom_field_desc_t interpolation_lagrange_config_fields[] = {
    {"order", FIELD_INT, offsetof(interpolation_lagrange_params_t, order)},
};
FIELD_COUNT(interpolation_lagrange_config_fields);

const atom_field_desc_t interpolation_lagrange_state_fields[] = {
    {"buffer", FIELD_BUFFER, offsetof(interpolation_lagrange_state_t, buffer), 16u},
    {"write_pos", FIELD_INT, offsetof(interpolation_lagrange_state_t, write_pos)},
};
FIELD_COUNT(interpolation_lagrange_state_fields);

const atom_field_desc_t interpolation_sinc_config_fields[] = {
    {"num_taps", FIELD_INT, offsetof(interpolation_sinc_params_t, num_taps)},
};
FIELD_COUNT(interpolation_sinc_config_fields);

const atom_field_desc_t interpolation_sinc_state_fields[] = {
    {"taps", FIELD_BUFFER, offsetof(interpolation_sinc_state_t, taps), 64u},
};
FIELD_COUNT(interpolation_sinc_state_fields);

#undef FIELD_COUNT
