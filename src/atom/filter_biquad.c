#include <atom/dsp_atoms.h>

#define CHUNK_LENGTH 512

void filter_biquad(filter_biquad_out_t out, filter_biquad_in_t in, filter_biquad_params_t params, filter_biquad_state_t *state) {
    if (out.signal == NULL || in.signal == NULL || state == NULL) return;

    float z1 = state->z1;
    float z2 = state->z2;

    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        float x0 = in.signal[i];
        float y0 = params.b0 * x0 + z1;
        z1 = params.b1 * x0 - params.a1 * y0 + z2;
        z2 = params.b2 * x0 - params.a2 * y0;
        out.signal[i] = y0;
    }

    state->z1 = z1;
    state->z2 = z2;
}
