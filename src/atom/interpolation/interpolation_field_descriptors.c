/* Generated from schema/atoms/atoms.json by tools/generate_atom_artifacts.pl. Do not edit. */
#include "atom/atom_field_descriptors.h"
#include "atom/dsp_atoms.h"

#include <stddef.h>

#define FIELD_COUNT(name) const int name##_count = (int)(sizeof(name) / sizeof((name)[0]))

// clang-format off
const atom_field_desc_t interpolation_lagrange_config_fields[] = {
    {"order", FIELD_INT, offsetof(interpolation_lagrange_params_t, order)},
};
FIELD_COUNT(interpolation_lagrange_config_fields);

const atom_field_desc_t interpolation_sinc_config_fields[] = {
    {"num_taps", FIELD_INT, offsetof(interpolation_sinc_params_t, num_taps)},
};
FIELD_COUNT(interpolation_sinc_config_fields);

// clang-format on

#undef FIELD_COUNT
