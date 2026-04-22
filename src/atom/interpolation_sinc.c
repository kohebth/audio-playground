#include <atom/dsp_atoms.h>

#define CHUNK_LENGTH 512
#include <math.h>
#include <stddef.h>

#define PI 3.14159265358979323846f

static float sinc(float x) {
    if (fabsf(x) < 1e-6f)
        return 1.0f;
    return sinf(PI * x) / (PI * x);
}

static float blackman_window(float n, float M) {
    return 0.42f - 0.5f * cosf(2.0f * PI * n / M) + 0.08f * cosf(4.0f * PI * n / M);
}

void interpolation_sinc(
    interpolation_sinc_out_t    *out,
    interpolation_sinc_in_t     *in,
    interpolation_sinc_params_t *params,
    interpolation_sinc_state_t  *state
) {
    if (out->signal == NULL || in->buffer == NULL || in->position == NULL)
        return;

    int taps = params->num_taps;
    if (taps % 2 == 0)
        taps++;
    int half_taps = taps / 2;

    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        float pos        = in->position[i];
        int   center_idx = (int)floorf(pos);
        float acc        = 0.0f;

        for (int k = -half_taps; k <= half_taps; ++k) {
            int   idx = center_idx + k;
            float x   = (float)idx - pos;
            float w   = 0.42f - 0.5f * cosf(2.0f * PI * (float)(k + half_taps) / (float)(taps - 1)) +
                      0.08f * cosf(4.0f * PI * (float)(k + half_taps) / (float)(taps - 1));

            float s;
            if (fabsf(x) < 1e-6f)
                s = 1.0f;
            else
                s = sinf(PI * x) / (PI * x);

            acc += in->buffer[idx] * s * w;
        }
        out->signal[i] = acc;
    }
}
