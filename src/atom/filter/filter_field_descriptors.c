#include "atom/atom_field_descriptors.h"
#include "atom/dsp_atoms.h"

#include <stddef.h>

#define FIELD_COUNT(name) const int name##_count = (int)(sizeof(name) / sizeof((name)[0]))

const atom_field_desc_t filter_allpass_config_fields[] = {
    {"delay_samples",   FIELD_INT, offsetof(filter_allpass_params_t, delay_samples)},
    {  "coefficient", FIELD_FLOAT, offsetof(filter_allpass_params_t,   coefficient)},
};
FIELD_COUNT(filter_allpass_config_fields);

const atom_field_desc_t filter_allpass_state_fields[] = {
    {"buffer", FIELD_BUFFER, offsetof(filter_allpass_state_t, buffer), 48000u},
    {"write_pos", FIELD_INT, offsetof(filter_allpass_state_t, write_pos)},
};
FIELD_COUNT(filter_allpass_state_fields);

const atom_field_desc_t filter_biquad_coefficients_config_fields[] = {
    {"b0", FIELD_FLOAT, offsetof(filter_biquad_coefficients_params_t, b0)},
    {"b1", FIELD_FLOAT, offsetof(filter_biquad_coefficients_params_t, b1)},
    {"b2", FIELD_FLOAT, offsetof(filter_biquad_coefficients_params_t, b2)},
    {"a1", FIELD_FLOAT, offsetof(filter_biquad_coefficients_params_t, a1)},
    {"a2", FIELD_FLOAT, offsetof(filter_biquad_coefficients_params_t, a2)},
};
FIELD_COUNT(filter_biquad_coefficients_config_fields);

const atom_field_desc_t filter_biquad_coefficients_state_fields[] = {
    {"z1", FIELD_FLOAT, offsetof(filter_biquad_coefficients_state_t, z1)},
    {"z2", FIELD_FLOAT, offsetof(filter_biquad_coefficients_state_t, z2)},
};
FIELD_COUNT(filter_biquad_coefficients_state_fields);

const atom_field_desc_t filter_biquad_in_fields[] = {
    {"signal", FIELD_SIGNAL, offsetof(filter_biquad_in_t, signal)},
    {"cutoff", FIELD_SIGNAL, offsetof(filter_biquad_in_t, cutoff)},
};
FIELD_COUNT(filter_biquad_in_fields);

const atom_field_desc_t filter_biquad_config_fields[] = {
    {      "cutoff", FIELD_FLOAT, offsetof(filter_biquad_params_t,       cutoff)},
    {           "q", FIELD_FLOAT, offsetof(filter_biquad_params_t,            q)},
    {        "mode",   FIELD_INT, offsetof(filter_biquad_params_t,         mode)},
    { "sample_rate", FIELD_FLOAT, offsetof(filter_biquad_params_t,  sample_rate)},
    {"smoothing_ms", FIELD_FLOAT, offsetof(filter_biquad_params_t, smoothing_ms)},
};
FIELD_COUNT(filter_biquad_config_fields);

const atom_field_desc_t filter_biquad_state_fields[] = {
    {            "z1", FIELD_FLOAT, offsetof(filter_biquad_state_t,             z1)},
    {            "z2", FIELD_FLOAT, offsetof(filter_biquad_state_t,             z2)},
    {"current_cutoff", FIELD_FLOAT, offsetof(filter_biquad_state_t, current_cutoff)},
    {     "current_q", FIELD_FLOAT, offsetof(filter_biquad_state_t,      current_q)},
};
FIELD_COUNT(filter_biquad_state_fields);

const atom_field_desc_t filter_comb_fb_config_fields[] = {
    {"delay_samples",   FIELD_INT, offsetof(filter_comb_fb_params_t, delay_samples)},
    {  "coefficient", FIELD_FLOAT, offsetof(filter_comb_fb_params_t,   coefficient)},
};
FIELD_COUNT(filter_comb_fb_config_fields);

const atom_field_desc_t filter_comb_fb_state_fields[] = {
    {"buffer", FIELD_BUFFER, offsetof(filter_comb_fb_state_t, buffer), 48000u},
    {"write_pos", FIELD_INT, offsetof(filter_comb_fb_state_t, write_pos)},
};
FIELD_COUNT(filter_comb_fb_state_fields);

const atom_field_desc_t filter_comb_ff_config_fields[] = {
    {"delay_samples",   FIELD_INT, offsetof(filter_comb_ff_params_t, delay_samples)},
    {  "coefficient", FIELD_FLOAT, offsetof(filter_comb_ff_params_t,   coefficient)},
};
FIELD_COUNT(filter_comb_ff_config_fields);

const atom_field_desc_t filter_comb_ff_state_fields[] = {
    {"buffer", FIELD_BUFFER, offsetof(filter_comb_ff_state_t, buffer), 48000u},
    {"write_pos", FIELD_INT, offsetof(filter_comb_ff_state_t, write_pos)},
};
FIELD_COUNT(filter_comb_ff_state_fields);

const atom_field_desc_t filter_dc_block_config_fields[] = {
    {"coefficient", FIELD_FLOAT, offsetof(filter_dc_block_params_t, coefficient)},
};
FIELD_COUNT(filter_dc_block_config_fields);

const atom_field_desc_t filter_dc_block_state_fields[] = {
    { "prev_input", FIELD_FLOAT, offsetof(filter_dc_block_state_t,  prev_input)},
    {"prev_output", FIELD_FLOAT, offsetof(filter_dc_block_state_t, prev_output)},
};
FIELD_COUNT(filter_dc_block_state_fields);

const atom_field_desc_t filter_differentiate_state_fields[] = {
    {"prev_sample", FIELD_FLOAT, offsetof(filter_differentiate_state_t, prev_sample)},
};
FIELD_COUNT(filter_differentiate_state_fields);

const atom_field_desc_t filter_fir_config_fields[] = {
    {     "kernel", FIELD_BUFFER, offsetof(filter_fir_params_t,      kernel)},
    {"kernel_size",    FIELD_INT, offsetof(filter_fir_params_t, kernel_size)},
};
FIELD_COUNT(filter_fir_config_fields);

const atom_field_desc_t filter_fir_state_fields[] = {
    {"buffer", FIELD_BUFFER, offsetof(filter_fir_state_t, buffer), 1024u},
    {"write_pos", FIELD_INT, offsetof(filter_fir_state_t, write_pos)},
};
FIELD_COUNT(filter_fir_state_fields);

const atom_field_desc_t filter_integrate_state_fields[] = {
    {"accumulator", FIELD_FLOAT, offsetof(filter_integrate_state_t, accumulator)},
};
FIELD_COUNT(filter_integrate_state_fields);

#undef FIELD_COUNT
