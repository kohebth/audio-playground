#include <atom/dsp_atoms.h>
#include <stddef.h>

#define CHUNK_LENGTH 512

void amplitude_multiply(
    amplitude_multiply_out_t    *out,
    amplitude_multiply_in_t     *in,
    amplitude_multiply_params_t *params,
    amplitude_multiply_state_t  *state
) {
    if (out->signal == NULL || in->signal_a == NULL || in->signal_b == NULL)
        return;
    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        out->signal[i] = in->signal_a[i] * in->signal_b[i];
    }
}
