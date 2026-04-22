#include <atom/dsp_atoms.h>
#include <stddef.h>

#define CHUNK_LENGTH 512

void filter_differentiate(
    filter_differentiate_out_t    *out,
    filter_differentiate_in_t     *in,
    filter_differentiate_params_t *params,
    filter_differentiate_state_t  *state
) {
    if (out->signal == NULL || in->signal == NULL || state == NULL)
        return;

    float last = state->prev_sample;
    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        float x0       = in->signal[i];
        out->signal[i] = x0 - last;
        last           = x0;
    }
    state->prev_sample = last;
}
