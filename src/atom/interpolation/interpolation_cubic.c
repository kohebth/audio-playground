#include <atom/dsp_atoms.h>
#include <apgcore/dsp/dsp_safety.h>
#include <stddef.h>

void interpolation_cubic_process(
    interpolation_cubic_out_t    *out,
    interpolation_cubic_in_t     *in,
    interpolation_cubic_params_t *params,
    interpolation_cubic_state_t  *state,
    const apg_process_info_t     *info
) {
    (void)params;
    (void)state;
    if (out == NULL || in == NULL || out->signal == NULL || in->signal_n1 == NULL || in->signal_a == NULL ||
        in->signal_b == NULL || in->signal_c == NULL || in->t == NULL)
        return;

    const uint32_t frames = apg_process_frames_or_default(info);
    for (uint32_t i = 0; i < frames; ++i) {
        const float t  = apg_clamp_float(in->t[i], 0.0f, 1.0f);
        const float t2 = t * t;
        const float t3 = t2 * t;

        const float v0 = isfinite(in->signal_n1[i]) ? in->signal_n1[i] : 0.0f;
        const float v1 = isfinite(in->signal_a[i]) ? in->signal_a[i] : 0.0f;
        const float v2 = isfinite(in->signal_b[i]) ? in->signal_b[i] : 0.0f;
        const float v3 = isfinite(in->signal_c[i]) ? in->signal_c[i] : 0.0f;

        const float a = -0.5f * v0 + 1.5f * v1 - 1.5f * v2 + 0.5f * v3;
        const float b = v0 - 2.5f * v1 + 2.0f * v2 - 0.5f * v3;
        const float c = -0.5f * v0 + 0.5f * v2;
        const float d = v1;

        const float sample = a * t3 + b * t2 + c * t + d;
        out->signal[i]     = isfinite(sample) ? sample : 0.0f;
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
