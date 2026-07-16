#include "atom/atom_field_descriptors.h"
#include "atom/dsp_atoms.h"

#include <stddef.h>

#define FIELD_COUNT(name) const int name##_count = (int)(sizeof(name) / sizeof((name)[0]))

const atom_field_desc_t src_antialias_config_fields[] = {
    {     "cutoff", FIELD_FLOAT, offsetof(src_antialias_params_t,      cutoff)},
    {"sample_rate", FIELD_FLOAT, offsetof(src_antialias_params_t, sample_rate)},
};
FIELD_COUNT(src_antialias_config_fields);

const atom_field_desc_t src_antialias_state_fields[] = {
    {"z1", FIELD_FLOAT, offsetof(src_antialias_state_t, z1)},
    {"z2", FIELD_FLOAT, offsetof(src_antialias_state_t, z2)},
};
FIELD_COUNT(src_antialias_state_fields);

const atom_field_desc_t src_antiimage_config_fields[] = {
    {     "cutoff", FIELD_FLOAT, offsetof(src_antiimage_params_t,      cutoff)},
    {"sample_rate", FIELD_FLOAT, offsetof(src_antiimage_params_t, sample_rate)},
};
FIELD_COUNT(src_antiimage_config_fields);

const atom_field_desc_t src_antiimage_state_fields[] = {
    {"z1", FIELD_FLOAT, offsetof(src_antiimage_state_t, z1)},
    {"z2", FIELD_FLOAT, offsetof(src_antiimage_state_t, z2)},
};
FIELD_COUNT(src_antiimage_state_fields);

const atom_field_desc_t src_convert_format_config_fields[] = {
    {"from_format", FIELD_INT, offsetof(src_convert_format_params_t, from_format)},
    {  "to_format", FIELD_INT, offsetof(src_convert_format_params_t,   to_format)},
};
FIELD_COUNT(src_convert_format_config_fields);

const atom_field_desc_t src_downsample_config_fields[] = {
    {"factor", FIELD_INT, offsetof(src_downsample_params_t, factor)},
};
FIELD_COUNT(src_downsample_config_fields);

const atom_field_desc_t src_downsample_state_fields[] = {
    {"phase", FIELD_INT, offsetof(src_downsample_state_t, phase)},
};
FIELD_COUNT(src_downsample_state_fields);

const atom_field_desc_t src_upsample_config_fields[] = {
    {"factor", FIELD_INT, offsetof(src_upsample_params_t, factor)},
};
FIELD_COUNT(src_upsample_config_fields);

const atom_field_desc_t src_upsample_state_fields[] = {
    {"phase", FIELD_INT, offsetof(src_upsample_state_t, phase)},
};
FIELD_COUNT(src_upsample_state_fields);

#undef FIELD_COUNT
