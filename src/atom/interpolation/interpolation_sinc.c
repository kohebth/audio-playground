#include <atom/dsp_atoms.h>
#include <math.h>
#include <stddef.h>

#define PI 3.14159265358979323846f

void interpolation_sinc_process(
    interpolation_sinc_out_t    *out,
    interpolation_sinc_in_t     *in,
    interpolation_sinc_params_t *params,
    interpolation_sinc_state_t  *state,
    const apg_process_info_t    *info
) {
    if (out == NULL || in == NULL || params == NULL || state == NULL)
        return;
    if (out->signal == NULL || in->buffer == NULL || in->position == NULL)
        return;

    int taps = params->num_taps;
    if (taps % 2 == 0)
        taps++;
    int half_taps = taps / 2;

    const uint32_t frames = apg_process_frames_or_default(info);
    for (uint32_t i = 0; i < frames; ++i) {
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

void interpolation_sinc(
    interpolation_sinc_out_t    *out,
    interpolation_sinc_in_t     *in,
    interpolation_sinc_params_t *params,
    interpolation_sinc_state_t  *state
) {
    const apg_process_info_t info = apg_process_info_default();
    interpolation_sinc_process(out, in, params, state, &info);
}
