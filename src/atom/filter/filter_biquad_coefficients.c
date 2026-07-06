#include <atom/dsp_atoms.h>
#include <stddef.h>

void filter_biquad_coefficients_process(
    filter_biquad_coefficients_out_t    *out,
    filter_biquad_coefficients_in_t     *in,
    filter_biquad_coefficients_params_t *params,
    filter_biquad_coefficients_state_t  *state,
    const apg_process_info_t            *info
) {
    if (out->signal == NULL || in->signal == NULL || params == NULL || state == NULL)
        return;

    float z1 = state->z1;
    float z2 = state->z2;

    const uint32_t frames = apg_process_frames_or_default(info);
    for (uint32_t i = 0; i < frames; ++i) {
        float x0       = in->signal[i];
        float y0       = params->b0 * x0 + z1;
        z1             = params->b1 * x0 - params->a1 * y0 + z2;
        z2             = params->b2 * x0 - params->a2 * y0;
        out->signal[i] = y0;
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
