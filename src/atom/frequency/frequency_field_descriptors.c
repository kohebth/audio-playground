#include "atom/atom_field_descriptors.h"
#include "atom/dsp_atoms.h"

#include <stddef.h>

#define FIELD_COUNT(name) const int name##_count = (int)(sizeof(name) / sizeof((name)[0]))

const atom_field_desc_t freq_fft_config_fields[] = {
    {"block_size", FIELD_INT, offsetof(freq_fft_params_t, block_size)},
};
FIELD_COUNT(freq_fft_config_fields);

const atom_field_desc_t freq_fft_state_fields[] = {
    {"workspace", FIELD_BUFFER, offsetof(freq_fft_state_t, workspace), 4096u},
    {"buffer_len", FIELD_INT, offsetof(freq_fft_state_t, buffer_len)},
};
FIELD_COUNT(freq_fft_state_fields);

const atom_field_desc_t freq_ifft_config_fields[] = {
    {"block_size", FIELD_INT, offsetof(freq_ifft_params_t, block_size)},
};
FIELD_COUNT(freq_ifft_config_fields);

const atom_field_desc_t freq_ifft_state_fields[] = {
    {"workspace", FIELD_BUFFER, offsetof(freq_ifft_state_t, workspace), 4096u},
    {"buffer_len", FIELD_INT, offsetof(freq_ifft_state_t, buffer_len)},
};
FIELD_COUNT(freq_ifft_state_fields);

const atom_field_desc_t freq_multiply_config_fields[] = {
    {"block_size", FIELD_INT, offsetof(freq_multiply_params_t, block_size)},
};
FIELD_COUNT(freq_multiply_config_fields);

const atom_field_desc_t freq_overlap_add_config_fields[] = {
    {"block_size", FIELD_INT, offsetof(freq_overlap_add_params_t, block_size)},
    {  "hop_size", FIELD_INT, offsetof(freq_overlap_add_params_t,   hop_size)},
};
FIELD_COUNT(freq_overlap_add_config_fields);

const atom_field_desc_t freq_overlap_add_state_fields[] = {
    {"buffer", FIELD_BUFFER, offsetof(freq_overlap_add_state_t, buffer), 8192u},
};
FIELD_COUNT(freq_overlap_add_state_fields);

const atom_field_desc_t freq_overlap_save_config_fields[] = {
    {"block_size", FIELD_INT, offsetof(freq_overlap_save_params_t, block_size)},
    {  "hop_size", FIELD_INT, offsetof(freq_overlap_save_params_t,   hop_size)},
};
FIELD_COUNT(freq_overlap_save_config_fields);

const atom_field_desc_t freq_overlap_save_state_fields[] = {
    {"buffer", FIELD_BUFFER, offsetof(freq_overlap_save_state_t, buffer), 1024u},
    {"write_pos", FIELD_INT, offsetof(freq_overlap_save_state_t, write_pos)},
};
FIELD_COUNT(freq_overlap_save_state_fields);

const atom_field_desc_t freq_window_config_fields[] = {
    {"window_type", FIELD_INT, offsetof(freq_window_params_t, window_type)},
    { "block_size", FIELD_INT, offsetof(freq_window_params_t,  block_size)},
};
FIELD_COUNT(freq_window_config_fields);

const atom_field_desc_t freq_shift_config_fields[] = {
    {"block_size", FIELD_INT, offsetof(freq_shift_params_t, block_size)},
};
FIELD_COUNT(freq_shift_config_fields);

const atom_field_desc_t freq_shift_state_fields[] = {
    {"window", FIELD_BUFFER, offsetof(freq_shift_state_t, window), 8192u},
    {"real", FIELD_BUFFER, offsetof(freq_shift_state_t, real), 8192u},
    {"imag", FIELD_BUFFER, offsetof(freq_shift_state_t, imag), 8192u},
    {"write_pos", FIELD_INT, offsetof(freq_shift_state_t, write_pos)},
    {"read_ptr", FIELD_FLOAT, offsetof(freq_shift_state_t, read_ptr)},
};
FIELD_COUNT(freq_shift_state_fields);

#undef FIELD_COUNT
