#include <atom/dsp_atoms.h>
#include <apgcore/dsp/dsp_safety.h>
#include <stddef.h>

void filter_biquad_coefficients_process(
    filter_biquad_coefficients_out_t    *out,
    filter_biquad_coefficients_in_t     *in,
    filter_biquad_coefficients_params_t *params,
    filter_biquad_coefficients_state_t  *state,
    const apg_process_info_t            *info
) {
    if (out == NULL || in == NULL || out->signal == NULL || in->signal == NULL || params == NULL || state == NULL)
        return;

    const uint32_t frames = apg_process_frames_or_default(info);
    const int coefficients_valid =
        apg_biquad_coefficients_are_finite(params->b0, params->b1, params->b2, params->a1, params->a2) &&
        apg_biquad_denominator_is_stable(params->a1, params->a2);

    if (!coefficients_valid) {
        for (uint32_t i = 0; i < frames; ++i)
            out->signal[i] = in->signal[i];
        state->z1 = 0.0f;
        state->z2 = 0.0f;
        return;
    }

    float z1 = apg_denormal_kill(state->z1);
    float z2 = apg_denormal_kill(state->z2);

    for (uint32_t i = 0; i < frames; ++i) {
        const float x0 = isfinite(in->signal[i]) ? in->signal[i] : 0.0f;
        float       y0 = params->b0 * x0 + z1;
        z1             = params->b1 * x0 - params->a1 * y0 + z2;
        z2             = params->b2 * x0 - params->a2 * y0;

        if (!isfinite(y0) || !isfinite(z1) || !isfinite(z2)) {
            y0 = x0;
            z1 = 0.0f;
            z2 = 0.0f;
        }

        out->signal[i] = apg_denormal_kill(y0);
        z1             = apg_denormal_kill(z1);
        z2             = apg_denormal_kill(z2);
    }

    state->z1 = z1;
    state->z2 = z2;
}

void filter_biquad_coefficients(
    filter_biquad_coefficients_out_t    *out,
    filter_biquad_coefficients_in_t     *in,
    filter_biquad_coefficients_params_t *params,
    filter_biquad_coefficients_state_t  *state
) {
    apg_process_info_t info = apg_process_info_default();
    filter_biquad_coefficients_process(out, in, params, state, &info);
}
