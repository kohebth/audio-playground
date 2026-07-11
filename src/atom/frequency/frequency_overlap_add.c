#include <atom/dsp_atoms.h>
#include <math.h>
#include <string.h>

#define MAX_OVERLAP_WINDOW 8192

void freq_overlap_add_spectral_process(
    freq_overlap_add_out_t    *out,
    freq_overlap_add_in_t     *in,
    freq_overlap_add_params_t *params,
    freq_overlap_add_state_t  *state,
    const apg_spectral_info_t *spectral_info
) {
    (void)params;
    if (!out || !in || !state || !out->signal || !in->frame || !state->buffer ||
        !apg_spectral_info_valid(spectral_info))
        return;

    const uint32_t n = spectral_info->fft_size;
    const uint32_t h = spectral_info->hop_size;
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
    freq_overlap_add_out_t    *out,
    freq_overlap_add_in_t     *in,
    freq_overlap_add_params_t *params,
    freq_overlap_add_state_t  *state,
    const apg_process_info_t  *info
) {
    if (out == NULL || in == NULL || params == NULL || state == NULL)
        return;
    if (out->signal == NULL || in->frame == NULL || state->buffer == NULL)
        return;

    const uint32_t frames = apg_process_frames_or_default(info);

    int block_size = params->block_size;
    if (block_size < 0)
        block_size = 0;
    if (block_size > MAX_OVERLAP_WINDOW)
        block_size = MAX_OVERLAP_WINDOW;

    int hop_size = params->hop_size;
    if (hop_size < 0)
        hop_size = 0;
    if (hop_size > MAX_OVERLAP_WINDOW)
        hop_size = MAX_OVERLAP_WINDOW;
    if ((uint32_t)hop_size > frames)
        hop_size = (int)frames;

    const uint32_t input_frames = frames < (uint32_t)block_size ? frames : (uint32_t)block_size;
    for (uint32_t i = 0; i < input_frames; ++i)
        state->buffer[i] += in->frame[i];

    for (int i = 0; i < hop_size; ++i)
        out->signal[i] = state->buffer[i];

    if (hop_size > 0) {
        memmove(state->buffer, state->buffer + hop_size, (MAX_OVERLAP_WINDOW - (size_t)hop_size) * sizeof(float));
        memset(state->buffer + (MAX_OVERLAP_WINDOW - hop_size), 0, (size_t)hop_size * sizeof(float));
    }
}

void freq_overlap_add(
    freq_overlap_add_out_t    *out,
    freq_overlap_add_in_t     *in,
    freq_overlap_add_params_t *params,
    freq_overlap_add_state_t  *state
) {
    const apg_process_info_t info = apg_process_info_default();
    freq_overlap_add_process(out, in, params, state, &info);
}
