/* Generated from schema/atoms/atoms.json by codegen/generate_atom_artifacts.pl. Do not edit. */
#include "atom/atom_field_descriptors.h"
#include "atom/dsp_atoms.h"

#include <stddef.h>

#define FIELD_COUNT(name) const int name##_count = (int)(sizeof(name) / sizeof((name)[0]))

// clang-format off
const atom_field_desc_t math_difference_state_fields[] = {
    {"prev_sample", FIELD_FLOAT, offsetof(math_difference_state_t, prev_sample)},
};
FIELD_COUNT(math_difference_state_fields);

const atom_field_desc_t math_integrate_config_fields[] = {
    {"leakage", FIELD_FLOAT, offsetof(math_integrate_params_t, leakage)},
};
FIELD_COUNT(math_integrate_config_fields);

const atom_field_desc_t math_integrate_state_fields[] = {
    {"accumulator", FIELD_FLOAT, offsetof(math_integrate_state_t, accumulator)},
};
FIELD_COUNT(math_integrate_state_fields);

// clang-format on

#undef FIELD_COUNT
