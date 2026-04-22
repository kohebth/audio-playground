#include <atom/dsp_atoms.h>
#include <math.h>
#include <stddef.h>

#define CHUNK_LENGTH 512

void src_antiimage(
    src_antiimage_out_t *out, src_antiimage_in_t *in, src_antiimage_params_t *params, src_antiimage_state_t *state
) {
    if (out->signal == NULL || in->signal == NULL || state == NULL)
        return;

    float ff  = params->cutoff / params->sample_rate;
    float ita = 1.0f / tanf(M_PI * ff);
    float q   = M_SQRT1_2;
    float b0  = 1.0f / (1.0f + ita / q + ita * ita);
    float b1  = 2.0f * b0;
    float b2  = b0;
    float a1  = 2.0f * (1.0f - ita * ita) * b0;
    float a2  = (1.0f - ita / q + ita * ita) * b0;

    float z1 = state->z1;
    float z2 = state->z2;

    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        float x0       = in->signal[i];
        float y0       = b0 * x0 + z1;
        z1             = b1 * x0 - a1 * y0 + z2;
        z2             = b2 * x0 - a2 * y0;
        out->signal[i] = y0;
    }

    state->z1 = z1;
    state->z2 = z2;
}
