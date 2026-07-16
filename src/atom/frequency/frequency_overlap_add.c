#include <atom/dsp_atoms.h>
#include <math.h>
#include <string.h>

void freq_overlap_add_spectral_process(
    freq_overlap_add_out_t          *out,
    const freq_overlap_add_in_t     *in,
    const freq_overlap_add_params_t *params,
    freq_overlap_add_state_t        *state,
    const apg_spectral_info_t       *spectral_info
) {
    (void)params;
    if (!out || !in || !state || !out->signal || !in->frame || !state->buffer ||
        !apg_spectral_info_valid(spectral_info))
        return;

    const uint32_t n = spectral_info->fft_size;
    const uint32_t h = spectral_info->hop_size;
    if (state->buffer_len < n)
        return;
    for (uint32_t i = 0; i < n; i++) {
        const float prior  = isfinite(state->buffer[i]) ? state->buffer[i] : 0.0f;
        const float sample = isfinite(in->frame[i]) ? in->frame[i] : 0.0f;
        state->buffer[i]   = prior + sample;
    }
    for (uint32_t i = 0; i < h; i++)
        out->signal[i] = state->buffer[i];
    memmove(state->buffer, state->buffer + h, (size_t)(n - h) * sizeof(float));
    memset(state->buffer + (n - h), 0, (size_t)h * sizeof(float));
}

void freq_overlap_add_process(
    freq_overlap_add_out_t          *out,
    const freq_overlap_add_in_t     *in,
    const freq_overlap_add_params_t *params,
    freq_overlap_add_state_t        *state,
    const apg_process_context_t     *info
) {
    if (!apg_process_context_valid(info))
        return;
    if (out == NULL || in == NULL || params == NULL || state == NULL || out->signal == NULL || in->frame == NULL ||
        state->buffer == NULL || state->buffer_len == 0u)
        return;

    const uint32_t frames   = apg_process_context_frames(info);
    const uint32_t capacity = state->buffer_len;

    uint32_t block_size = params->block_size > 0 ? (uint32_t)params->block_size : 0u;
    if (block_size > capacity)
        block_size = capacity;

    uint32_t hop_size = params->hop_size > 0 ? (uint32_t)params->hop_size : 0u;
    if (hop_size > capacity)
        hop_size = capacity;
    if (hop_size > frames)
        hop_size = frames;

    const uint32_t input_frames = frames < block_size ? frames : block_size;
    for (uint32_t i = 0; i < input_frames; ++i)
        state->buffer[i] += in->frame[i];

    for (uint32_t i = 0; i < hop_size; ++i)
        out->signal[i] = state->buffer[i];

    if (hop_size > 0u) {
        memmove(state->buffer, state->buffer + hop_size, (size_t)(capacity - hop_size) * sizeof(float));
        memset(state->buffer + (capacity - hop_size), 0, (size_t)hop_size * sizeof(float));
    }
}
