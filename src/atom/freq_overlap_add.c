#include <atom/dsp_atoms.h>
#include <stdlib.h>
#include <string.h>

#define MAX_OVERLAP_WINDOW 8192
#define CHUNK_LENGTH       512

void freq_overlap_add(
    freq_overlap_add_out_t    *out,
    freq_overlap_add_in_t     *in,
    freq_overlap_add_params_t *params,
    freq_overlap_add_state_t  *state
) {
    if (out->signal == NULL || in->frame == NULL || state == NULL || state->buffer == NULL)
        return;

    int N = params->block_size;
    int H = params->hop_size;
    if (N > MAX_OVERLAP_WINDOW)
        N = MAX_OVERLAP_WINDOW;
    if (H > CHUNK_LENGTH)
        H = CHUNK_LENGTH;

    for (int i = 0; i < N; i++) {
        state->buffer[i] += in->frame[i];
    }

    for (int i = 0; i < H; i++) {
        out->signal[i] = state->buffer[i];
    }

    memmove(state->buffer, state->buffer + H, (MAX_OVERLAP_WINDOW - H) * sizeof(float));
    memset(state->buffer + (MAX_OVERLAP_WINDOW - H), 0, H * sizeof(float));
}
