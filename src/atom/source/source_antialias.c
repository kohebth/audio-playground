#include <apgcore/dsp/dsp_safety.h>
#include <atom/dsp_atoms.h>
#include <math.h>
#include <stddef.h>

void src_antialias_process(
    src_antialias_out_t      *out,
    src_antialias_in_t       *in,
    src_antialias_params_t   *params,
    src_antialias_state_t    *state,
    const apg_process_info_t *info
) {
    if (out == NULL || in == NULL || params == NULL || state == NULL || out->signal == NULL || in->signal == NULL)
        return;

    const float    sample_rate = apg_sample_rate_or_default(info);
    const float    cutoff      = apg_clamp_float(params->cutoff, 1.0f, sample_rate * 0.49f);
    const float    ff          = cutoff / sample_rate;
    const float    ita         = 1.0f / tanf((float)M_PI * ff);
    const float    q           = (float)M_SQRT1_2;
    const float    b0          = 1.0f / (1.0f + ita / q + ita * ita);
    const float    b1          = 2.0f * b0;
    const float    b2          = b0;
    const float    a1          = 2.0f * (1.0f - ita * ita) * b0;
    const float    a2          = (1.0f - ita / q + ita * ita) * b0;
    const uint32_t frames      = info != NULL ? info->frames : APG_DEFAULT_FRAMES;

    if (!apg_biquad_coefficients_are_finite(b0, b1, b2, a1, a2) || !apg_biquad_denominator_is_stable(a1, a2)) {
        for (uint32_t i = 0; i < frames; ++i)
            out->signal[i] = 0.0f;
        state->z1 = 0.0f;
        state->z2 = 0.0f;
        return;
    }

    float z1 = isfinite(state->z1) ? state->z1 : 0.0f;
    float z2 = isfinite(state->z2) ? state->z2 : 0.0f;

    for (uint32_t i = 0; i < frames; ++i) {
        const float x0 = isfinite(in->signal[i]) ? in->signal[i] : 0.0f;
        float       y0 = b0 * x0 + z1;
        z1             = b1 * x0 - a1 * y0 + z2;
        z2             = b2 * x0 - a2 * y0;
        if (!isfinite(y0) || !isfinite(z1) || !isfinite(z2)) {
            y0 = 0.0f;
            z1 = 0.0f;
            z2 = 0.0f;
        }
        out->signal[i] = y0;
    }

    state->z1 = apg_denormal_kill(z1);
    state->z2 = apg_denormal_kill(z2);
}

void src_antialias(
    src_antialias_out_t *out, src_antialias_in_t *in, src_antialias_params_t *params, src_antialias_state_t *state
) {
    const apg_process_info_t info = apg_process_info_default();
    src_antialias_process(out, in, params, state, &info);
}
