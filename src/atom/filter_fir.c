#include <atom/dsp_atoms.h>
#include <stdlib.h>
#include <string.h>

#define CHUNK_LENGTH 512
#define MAX_FIR_SIZE 1024

void filter_fir(filter_fir_out_t out, filter_fir_in_t in, filter_fir_params_t params, filter_fir_state_t *state) {
    if (out.signal == NULL || in.signal == NULL || state == NULL || state->buffer == NULL) return;

    int k_size = params.kernel_size;
    if (k_size > MAX_FIR_SIZE) k_size = MAX_FIR_SIZE;
    int write_pos = state->write_pos;

    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        state->buffer[write_pos] = in.signal[i];

        float acc = 0.0f;
        for (int k = 0; k < k_size; ++k) {
            int read_pos = write_pos - k;
            if (read_pos < 0) read_pos += MAX_FIR_SIZE;
            acc += state->buffer[read_pos] * params.kernel[k];
        }

        out.signal[i] = acc;
        write_pos = (write_pos + 1) % MAX_FIR_SIZE;
    }

    state->write_pos = write_pos;
}
