#include "atom/atom_field_descriptors.h"
#include "atom/dsp_atoms.h"

#include <stddef.h>

#define FIELD_COUNT(name) const int name##_count = (int)(sizeof(name) / sizeof((name)[0]))

const atom_field_desc_t mix_crossfade_config_fields[] = {
    {"t", FIELD_FLOAT, offsetof(mix_crossfade_params_t, t)},
};
FIELD_COUNT(mix_crossfade_config_fields);

const atom_field_desc_t mix_matrix_config_fields[] = {
    { "num_in", FIELD_INT, offsetof(mix_matrix_params_t,  num_in)},
    {"num_out", FIELD_INT, offsetof(mix_matrix_params_t, num_out)},
};
FIELD_COUNT(mix_matrix_config_fields);

const atom_field_desc_t mix_pan_stereo_config_fields[] = {
    {"position", FIELD_FLOAT, offsetof(mix_pan_stereo_params_t, position)},
};
FIELD_COUNT(mix_pan_stereo_config_fields);

const atom_field_desc_t mix_wet_dry_config_fields[] = {
    {"mix", FIELD_FLOAT, offsetof(mix_wet_dry_params_t, mix)},
};
FIELD_COUNT(mix_wet_dry_config_fields);

#undef FIELD_COUNT
