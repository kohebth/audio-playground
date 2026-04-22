#include <atom/dsp_atoms.h>
#include <stddef.h>

#define CHUNK_LENGTH 512

void amplitude_accumulate(
    amplitude_accumulate_out_t    *out,
    amplitude_accumulate_in_t     *in,
    amplitude_accumulate_params_t *params,
    amplitude_accumulate_state_t  *state
) {
    if (out->signal == NULL || in->signal == NULL || state == NULL)
        return;

    float sum = state->accumulator;
    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        sum += in->signal[i];
        out->signal[i] = sum;
    }
    state->accumulator = sum;
}
