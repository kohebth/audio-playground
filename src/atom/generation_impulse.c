#include <atom/dsp_atoms.h>
#include <stddef.h>
#include <stdint.h>

#define CHUNK_LENGTH 512

void generation_impulse(
    generation_impulse_out_t    *out,
    generation_impulse_in_t     *in,
    generation_impulse_params_t *params,
    generation_impulse_state_t  *state
) {
    if (out->signal == NULL || state == NULL)
        return;

    int interval_samples = (int)(params->interval * params->sample_rate);
    if (interval_samples < 1)
        interval_samples = 1;

    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        if (state->counter <= 0) {
            out->signal[i] = 1.0f;
            state->counter = interval_samples - 1;
        } else {
            out->signal[i] = 0.0f;
            state->counter--;
        }
    }
}
