#include <apgcore/dsp/dsp_safety.h>
#include <atom/dsp_atoms.h>
#include <math.h>
#include <string.h>

void freq_overlap_save_spectral_process(
    freq_overlap_save_out_t          *out,
    const freq_overlap_save_in_t     *in,
    const freq_overlap_save_params_t *params,
    freq_overlap_save_state_t        *state,
    const apg_spectral_info_t        *spectral_info
) {
    (void)params;
    if (!out || !in || !state || !out->frame || !in->signal || !state->buffer ||
        !apg_spectral_info_valid(spectral_info))
        return;

    const uint32_t n = spectral_info->fft_size;
    const uint32_t h = spectral_info->hop_size;
    if (state->buffer_len < n)
        return;
    memmove(state->buffer, state->buffer + h, (size_t)(n - h) * sizeof(float));
    for (uint32_t i = 0; i < h; i++)
        state->buffer[n - h + i] = isfinite(in->signal[i]) ? in->signal[i] : 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        if (!isfinite(state->buffer[i]))
            state->buffer[i] = 0.0f;
        out->frame[i] = state->buffer[i];
    }
    state->write_pos = 0;
}

void freq_overlap_save_process(
    freq_overlap_save_out_t          *out,
    const freq_overlap_save_in_t     *in,
    const freq_overlap_save_params_t *params,
    freq_overlap_save_state_t        *state,
    const apg_process_context_t      *info
) {
    if (!apg_process_context_valid(info))
        return;
    if (out == NULL || in == NULL || params == NULL || state == NULL || out->frame == NULL || in->signal == NULL ||
        state->buffer == NULL || state->buffer_len == 0u)
        return;

    const uint32_t frames   = apg_process_context_frames(info);
    const uint32_t capacity = state->buffer_len;

    uint32_t block_size = params->block_size > 0 ? (uint32_t)params->block_size : 0u;
    if (block_size > capacity)
        block_size = capacity;
    if (block_size > frames)
        block_size = frames;

    uint32_t hop_size = params->hop_size > 0 ? (uint32_t)params->hop_size : 0u;
    if (hop_size > capacity)
        hop_size = capacity;
    if (hop_size > frames)
        hop_size = frames;

    uint32_t write_pos = apg_wrap_index_i64(state->write_pos, capacity);
    for (uint32_t i = 0; i < hop_size; ++i) {
        state->buffer[write_pos] = in->signal[i];
        write_pos                = write_pos + 1u == capacity ? 0u : write_pos + 1u;
    }

    for (uint32_t i = 0; i < block_size; ++i) {
        const uint32_t read_pos = apg_wrap_index_i64((int64_t)write_pos - (int64_t)block_size + (int64_t)i, capacity);
        out->frame[i]           = state->buffer[read_pos];
    }

    state->write_pos = (int)write_pos;
}
