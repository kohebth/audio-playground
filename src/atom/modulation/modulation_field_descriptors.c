#include "atom/atom_field_descriptors.h"
#include "atom/dsp_atoms.h"

#include <stddef.h>

#define FIELD_COUNT(name) const int name##_count = (int)(sizeof(name) / sizeof((name)[0]))

const atom_field_desc_t modulation_amplitude_config_fields[] = {
    {"depth", FIELD_FLOAT, offsetof(modulation_amplitude_params_t, depth)},
};
FIELD_COUNT(modulation_amplitude_config_fields);

const atom_field_desc_t modulation_frequency_config_fields[] = {
    {"depth", FIELD_FLOAT, offsetof(modulation_frequency_params_t, depth)},
};
FIELD_COUNT(modulation_frequency_config_fields);

const atom_field_desc_t modulation_frequency_state_fields[] = {
    {"buffer", FIELD_BUFFER, offsetof(modulation_frequency_state_t, buffer), APG_MODULATION_DELAY_CAPACITY},
    {"write_pos", FIELD_INT, offsetof(modulation_frequency_state_t, write_pos)},
    {"current_delay", FIELD_FLOAT, offsetof(modulation_frequency_state_t, current_delay)},
};
FIELD_COUNT(modulation_frequency_state_fields);

const atom_field_desc_t modulation_phase_config_fields[] = {
    {"depth", FIELD_FLOAT, offsetof(modulation_phase_params_t, depth)},
};
FIELD_COUNT(modulation_phase_config_fields);

const atom_field_desc_t modulation_phase_state_fields[] = {
    {"buffer", FIELD_BUFFER, offsetof(modulation_phase_state_t, buffer), APG_MODULATION_DELAY_CAPACITY},
    {"write_pos", FIELD_INT, offsetof(modulation_phase_state_t, write_pos)},
};
FIELD_COUNT(modulation_phase_state_fields);

const atom_field_desc_t modulation_scrub_config_fields[] = {
    {"buffer_size", FIELD_INT, offsetof(modulation_scrub_params_t, buffer_size)},
};
FIELD_COUNT(modulation_scrub_config_fields);

#undef FIELD_COUNT
