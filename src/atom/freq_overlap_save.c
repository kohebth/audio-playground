#include <atom/dsp_atoms.h>
#include <string.h>
#include <stdlib.h>

#define MAX_FRAMES 16
#define MAX_FRAME_SIZE 1024

void freq_overlap_save(freq_overlap_save_out_t out, freq_overlap_save_in_t in, freq_overlap_save_params_t params, freq_overlap_save_state_t *state) {
    if (out.frame == NULL || in.signal == NULL || state == NULL || state->buffer == NULL) return;

    int N = params.block_size;
    int H = params.hop_size;
    if (N > MAX_FRAME_SIZE) N = MAX_FRAME_SIZE;
    int write_pos = state->write_pos;

    // Fill buffer with new samples
    for (int i = 0; i < H; i++) {
        state->buffer[write_pos] = in.signal[i];
        write_pos = (write_pos + 1) % MAX_FRAME_SIZE;
    }

    // Output frame (last N samples)
    for (int i = 0; i < N; i++) {
        int read_pos = (write_pos - N + i + MAX_FRAME_SIZE) % MAX_FRAME_SIZE;
        out.frame[i] = state->buffer[read_pos];
    }

    state->write_pos = write_pos;
}
