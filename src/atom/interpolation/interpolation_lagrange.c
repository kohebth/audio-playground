#include <atom/dsp_atoms.h>
#include <math.h>
#include <stddef.h>

void interpolation_lagrange_process(
    interpolation_lagrange_out_t    *out,
    interpolation_lagrange_in_t     *in,
    interpolation_lagrange_params_t *params,
    interpolation_lagrange_state_t  *state,
    const apg_process_info_t        *info
) {
    if (out->signal == NULL || in->samples == NULL || in->t == NULL)
        return;

    int            n      = params->order;
    const uint32_t frames = apg_process_frames_or_default(info);
    for (uint32_t i = 0; i < frames; ++i) {
        float pos  = in->t[i];
        int   base = (int)floorf(pos) - n / 2;
        float frac = pos - floorf(pos) + (float)(n / 2);

        float result = 0.0f;
        for (int k = 0; k <= n; ++k) {
            float l_k = 1.0f;
            for (int j = 0; j <= n; ++j) {
                if (k == j)
                    continue;
                l_k *= (frac - (float)j) / ((float)k - (float)j);
            }
            result += in->samples[base + k] * l_k;
        }
        out->signal[i] = result;
    }
}

void interpolation_lagrange(
    interpolation_lagrange_out_t    *out,
    interpolation_lagrange_in_t     *in,
    interpolation_lagrange_params_t *params,
    interpolation_lagrange_state_t  *state
) {
    const apg_process_info_t info = apg_process_info_default();
    interpolation_lagrange_process(out, in, params, state, &info);
}
