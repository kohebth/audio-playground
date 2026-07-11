#include <atom/dsp_atoms.h>
#include <math.h>
#include <string.h>

#define MAX_FRAME_SIZE 1024

void freq_overlap_save_spectral_process(
    freq_overlap_save_out_t    *out,
    freq_overlap_save_in_t     *in,
    freq_overlap_save_params_t *params,
    freq_overlap_save_state_t  *state,
    const apg_spectral_info_t  *spectral_info
) {
    (void)params;
    if (!out || !in || !state || !out->frame || !in->signal || !state->buffer ||
        !apg_spectral_info_valid(spectral_info))
        return;

    const uint32_t n = spectral_info->fft_size;
    const uint32_t h = spectral_info->hop_size;
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
    freq_overlap_save_out_t    *out,
    freq_overlap_save_in_t     *in,
    freq_overlap_save_params_t *params,
    freq_overlap_save_state_t  *state,
    const apg_process_info_t   *info
) {
    if (out == NULL || in == NULL || params == NULL || state == NULL)
        return;
    if (out->frame == NULL || in->signal == NULL || state->buffer == NULL)
        return;

    const uint32_t frames = apg_process_frames_or_default(info);

    int block_size = params->block_size;
    if (block_size < 0)
        block_size = 0;
    if (block_size > MAX_FRAME_SIZE)
        block_size = MAX_FRAME_SIZE;

    int hop_size = params->hop_size;
    if (hop_size < 0)
        hop_size = 0;
    if (hop_size > MAX_FRAME_SIZE)
        hop_size = MAX_FRAME_SIZE;
    if ((uint32_t)hop_size > frames)
        hop_size = (int)frames;

    if ((uint32_t)block_size > frames)
        block_size = (int)frames;

    int write_pos = state->write_pos;
    for (int i = 0; i < hop_size; ++i) {
        state->buffer[write_pos] = in->signal[i];
        write_pos                = (write_pos + 1) % MAX_FRAME_SIZE;
    }

    for (int i = 0; i < block_size; ++i) {
        int read_pos  = (write_pos - block_size + i + MAX_FRAME_SIZE) % MAX_FRAME_SIZE;
        out->frame[i] = state->buffer[read_pos];
    }

    state->write_pos = write_pos;
}

void freq_overlap_save(
    freq_overlap_save_out_t    *out,
    freq_overlap_save_in_t     *in,
    freq_overlap_save_params_t *params,
    freq_overlap_save_state_t  *state
) {
    const apg_process_info_t info = apg_process_info_default();
    freq_overlap_save_process(out, in, params, state, &info);
}
