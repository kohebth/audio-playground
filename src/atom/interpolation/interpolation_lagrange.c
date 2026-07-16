#include <apgcore/dsp/dsp_safety.h>
#include <atom/dsp_atoms.h>
#include <math.h>
#include <stddef.h>

#define APG_LAGRANGE_MAX_ORDER 8

void interpolation_lagrange_process(
    interpolation_lagrange_out_t          *out,
    const interpolation_lagrange_in_t     *in,
    const interpolation_lagrange_params_t *params,
    interpolation_lagrange_state_t        *state,
    const apg_process_context_t           *info
) {
    if (!apg_process_context_valid(info))
        return;
    if (out == NULL || in == NULL || params == NULL || state == NULL)
        return;
    if (out->signal == NULL || in->samples == NULL || in->t == NULL)
        return;

    const uint32_t frames = apg_process_context_frames(info);
    if (frames == 0u)
        return;

    int order = params->order;
    if (order < 0)
        order = 0;
    if (order > APG_LAGRANGE_MAX_ORDER)
        order = APG_LAGRANGE_MAX_ORDER;
    if ((uint32_t)order >= frames)
        order = (int)frames - 1;

    const int sample_count = order + 1;
    const int max_base     = (int)frames - sample_count;

    for (uint32_t i = 0; i < frames; ++i) {
        float position = in->t[i];
        if (!isfinite(position))
            position = 0.0f;
        position = apg_clamp_float(position, 0.0f, (float)(frames - 1u));

        int base = (int)floorf(position) - order / 2;
        if (base < 0)
            base = 0;
        if (base > max_base)
            base = max_base;
        const float x = position - (float)base;

        float result = 0.0f;
        for (int k = 0; k <= order; ++k) {
            float basis = 1.0f;
            for (int j = 0; j <= order; ++j) {
                if (k == j)
                    continue;
                basis *= (x - (float)j) / ((float)k - (float)j);
            }
            const float sample = isfinite(in->samples[base + k]) ? in->samples[base + k] : 0.0f;
            result += sample * basis;
        }
        out->signal[i] = isfinite(result) ? result : 0.0f;
    }
}
