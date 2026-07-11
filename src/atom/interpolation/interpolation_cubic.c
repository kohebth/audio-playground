#include <atom/dsp_atoms.h>
#include <stddef.h>

void interpolation_cubic_process(
    interpolation_cubic_out_t    *out,
    interpolation_cubic_in_t     *in,
    interpolation_cubic_params_t *params,
    interpolation_cubic_state_t  *state,
    const apg_process_info_t     *info
) {
    if (out == NULL || in == NULL || params == NULL || state == NULL)
        return;
    if (out->signal == NULL || in->signal_n1 == NULL || in->signal_a == NULL || in->signal_b == NULL ||
        in->signal_c == NULL || in->t == NULL)
        return;

    const uint32_t frames = apg_process_frames_or_default(info);

    for (uint32_t i = 0; i < frames; ++i) {
        float t  = in->t[i];
        float t2 = t * t;
        float t3 = t2 * t;

        float v0 = in->signal_n1[i];
        float v1 = in->signal_a[i];
        float v2 = in->signal_b[i];
        float v3 = in->signal_c[i];

        float a = -0.5f * v0 + 1.5f * v1 - 1.5f * v2 + 0.5f * v3;
        float b = v0 - 2.5f * v1 + 2.0f * v2 - 0.5f * v3;
        float c = -0.5f * v0 + 0.5f * v2;
        float d = v1;

        out->signal[i] = a * t3 + b * t2 + c * t + d;
    }
}

void interpolation_cubic(
    interpolation_cubic_out_t    *out,
    interpolation_cubic_in_t     *in,
    interpolation_cubic_params_t *params,
    interpolation_cubic_state_t  *state
) {
    const apg_process_info_t info = apg_process_info_default();
    interpolation_cubic_process(out, in, params, state, &info);
}
