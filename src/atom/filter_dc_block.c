#include <atom/dsp_atoms.h>

#define CHUNK_LENGTH 512

void filter_dc_block(filter_dc_block_out_t out, filter_dc_block_in_t in, filter_dc_block_params_t params, filter_dc_block_state_t *state) {
    if (out.signal == NULL || in.signal == NULL || state == NULL) return;

    float x1 = state->prev_input;
    float y1 = state->prev_output;
    float R = params.coefficient;

    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        float x0 = in.signal[i];
        float y0 = x0 - x1 + R * y1;
        out.signal[i] = y0;
        x1 = x0;
        y1 = y0;
    }

    state->prev_input = x1;
    state->prev_output = y1;
}
