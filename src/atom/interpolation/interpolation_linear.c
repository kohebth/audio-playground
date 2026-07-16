#include <apgcore/dsp/dsp_safety.h>
#include <atom/dsp_atoms.h>
#include <stddef.h>

void interpolation_linear_process(
    interpolation_linear_out_t          *out,
    const interpolation_linear_in_t     *in,
    const interpolation_linear_params_t *params,
    interpolation_linear_state_t        *state,
    const apg_process_info_t            *info
) {
    if (out == NULL || in == NULL || params == NULL || state == NULL)
        return;
    if (out->signal == NULL || in->signal_a == NULL || in->signal_b == NULL || in->t == NULL)
        return;

    const uint32_t frames = apg_process_frames_or_default(info);
    for (uint32_t i = 0; i < frames; ++i) {
        const float t  = apg_clamp_float(in->t[i], 0.0f, 1.0f);
        const float a  = isfinite(in->signal_a[i]) ? in->signal_a[i] : 0.0f;
        const float b  = isfinite(in->signal_b[i]) ? in->signal_b[i] : 0.0f;
        out->signal[i] = a * (1.0f - t) + b * t;
    }
}
