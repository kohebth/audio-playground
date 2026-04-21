#include <atom/dsp_atoms.h>

#define CHUNK_LENGTH 512

void delay_unit(delay_unit_out_t out, delay_unit_in_t in, void *params, delay_unit_state_t *state) {
    if (out.signal == NULL || in.signal == NULL || state == NULL) return;

    float last = state->prev_sample;
    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        float current = in.signal[i];
        out.signal[i] = last;
        last = current;
    }
    state->prev_sample = last;
}
