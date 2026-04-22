#include <atom/dsp_atoms.h>
#include <stddef.h>

#define CHUNK_LENGTH 512

void filter_integrate(
    filter_integrate_out_t    *out,
    filter_integrate_in_t     *in,
    filter_integrate_params_t *params,
    filter_integrate_state_t  *state
) {
    if (out->signal == NULL || in->signal == NULL || state == NULL)
        return;

    float       acc     = state->accumulator;
    const float leakage = 0.999f;

    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        acc            = in->signal[i] + leakage * acc;
        out->signal[i] = acc;
    }

    state->accumulator = acc;
}
