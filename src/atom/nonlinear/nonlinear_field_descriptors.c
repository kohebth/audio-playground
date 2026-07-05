#include "atom/atom_field_descriptors.h"
#include "atom/dsp_atoms.h"

#include <stddef.h>

#define FIELD_COUNT(name) const int name##_count = (int)(sizeof(name) / sizeof((name)[0]))

const atom_field_desc_t nonlinear_bitcrush_config_fields[] = {
    {"bit_depth", FIELD_FLOAT, offsetof(nonlinear_bitcrush_params_t, bit_depth)},
};
FIELD_COUNT(nonlinear_bitcrush_config_fields);

const atom_field_desc_t nonlinear_sample_hold_config_fields[] = {
    {"factor", FIELD_FLOAT, offsetof(nonlinear_sample_hold_params_t, factor)},
};
FIELD_COUNT(nonlinear_sample_hold_config_fields);

const atom_field_desc_t nonlinear_sample_hold_state_fields[] = {
    {"last_val", FIELD_FLOAT, offsetof(nonlinear_sample_hold_state_t, last_val)},
    { "counter", FIELD_FLOAT, offsetof(nonlinear_sample_hold_state_t,  counter)},
};
FIELD_COUNT(nonlinear_sample_hold_state_fields);

const atom_field_desc_t nonlinear_waveshape_config_fields[] = {
    {"transfer_table", FIELD_BUFFER, offsetof(nonlinear_waveshape_params_t, transfer_table)},
    {    "table_size",    FIELD_INT, offsetof(nonlinear_waveshape_params_t,     table_size)},
};
FIELD_COUNT(nonlinear_waveshape_config_fields);

#undef FIELD_COUNT
