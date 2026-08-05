/* Generated from schema/atoms/atoms.json by codegen/generate_atom_artifacts.pl. Do not edit. */
#include "atom/atom_field_descriptors.h"
#include "atom/dsp_atoms.h"

#include <stddef.h>

#define FIELD_COUNT(name) const int name##_count = (int)(sizeof(name) / sizeof((name)[0]))

// clang-format off
const atom_field_desc_t generation_dc_config_fields[] = {
    {"value", FIELD_FLOAT, offsetof(generation_dc_params_t, value)},
};
FIELD_COUNT(generation_dc_config_fields);

const atom_field_desc_t generation_envelope_config_fields[] = {
    {"attack", FIELD_FLOAT, offsetof(generation_envelope_params_t, attack)},
    {"decay", FIELD_FLOAT, offsetof(generation_envelope_params_t, decay)},
    {"sustain", FIELD_FLOAT, offsetof(generation_envelope_params_t, sustain)},
    {"release", FIELD_FLOAT, offsetof(generation_envelope_params_t, release)},
};
FIELD_COUNT(generation_envelope_config_fields);

const atom_field_desc_t generation_envelope_state_fields[] = {
    {"current_level", FIELD_FLOAT, offsetof(generation_envelope_state_t, current_level)},
    {"stage", FIELD_INT, offsetof(generation_envelope_state_t, stage)},
};
FIELD_COUNT(generation_envelope_state_fields);

const atom_field_desc_t generation_impulse_config_fields[] = {
    {"interval", FIELD_FLOAT, offsetof(generation_impulse_params_t, interval)},
};
FIELD_COUNT(generation_impulse_config_fields);

const atom_field_desc_t generation_impulse_state_fields[] = {
    {"counter", FIELD_INT, offsetof(generation_impulse_state_t, counter)},
};
FIELD_COUNT(generation_impulse_state_fields);

const atom_field_desc_t generation_lfo_config_fields[] = {
    {"frequency", FIELD_FLOAT, offsetof(generation_lfo_params_t, frequency)},
    {"waveform", FIELD_INT, offsetof(generation_lfo_params_t, waveform)},
    {"phase_offset", FIELD_FLOAT, offsetof(generation_lfo_params_t, phase_offset)},
};
FIELD_COUNT(generation_lfo_config_fields);

const atom_field_desc_t generation_lfo_state_fields[] = {
    {"phase", FIELD_FLOAT, offsetof(generation_lfo_state_t, phase)},
};
FIELD_COUNT(generation_lfo_state_fields);

const atom_field_desc_t generation_noise_config_fields[] = {
    {"amplitude", FIELD_FLOAT, offsetof(generation_noise_params_t, amplitude)},
    {"color", FIELD_INT, offsetof(generation_noise_params_t, color)},
};
FIELD_COUNT(generation_noise_config_fields);

const atom_field_desc_t generation_noise_state_fields[] = {
    {"seed", FIELD_INT, offsetof(generation_noise_state_t, seed)},
    {"prev_value", FIELD_FLOAT, offsetof(generation_noise_state_t, prev_value)},
};
FIELD_COUNT(generation_noise_state_fields);

const atom_field_desc_t generation_oscillator_config_fields[] = {
    {"frequency", FIELD_FLOAT, offsetof(generation_oscillator_params_t, frequency)},
    {"waveform", FIELD_INT, offsetof(generation_oscillator_params_t, waveform)},
    {"phase_offset", FIELD_FLOAT, offsetof(generation_oscillator_params_t, phase_offset)},
};
FIELD_COUNT(generation_oscillator_config_fields);

const atom_field_desc_t generation_oscillator_state_fields[] = {
    {"phase", FIELD_FLOAT, offsetof(generation_oscillator_state_t, phase)},
};
FIELD_COUNT(generation_oscillator_state_fields);

// clang-format on

#undef FIELD_COUNT
