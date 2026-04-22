#include <atom/dsp_atoms.h>
#include <stddef.h>

#define CHUNK_LENGTH 512

void detect_slope(
    detect_slope_out_t *out, detect_slope_in_t *in, detect_slope_params_t *params, detect_slope_state_t *state
) {
    if (out->slope == NULL || in->signal == NULL || state == NULL)
        return;

    float last = state->prev_sample;
    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        out->slope[i] = in->signal[i] - last;
        last          = in->signal[i];
    }
    state->prev_sample = last;
}
