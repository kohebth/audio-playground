#include <atom/dsp_atoms.h>
#include <math.h>
#include <stddef.h>

void amplitude_divide_process(
    amplitude_divide_out_t    *out,
    amplitude_divide_in_t     *in,
    amplitude_divide_params_t *params,
    amplitude_divide_state_t  *state,
    const apg_process_info_t  *info
) {
    if (out == NULL || in == NULL || params == NULL || state == NULL)
        return;
    (void)state;
    if (out->signal == NULL || in->numerator == NULL || in->denominator == NULL || params == NULL)
        return;

    const uint32_t frames = apg_process_frames_or_default(info);
    for (uint32_t i = 0; i < frames; ++i) {
        if (fabsf(in->denominator[i]) > params->epsilon) {
            out->signal[i] = in->numerator[i] / in->denominator[i];
        } else {
            out->signal[i] = 0.0f;
        }
    }
}

void amplitude_divide(
    amplitude_divide_out_t    *out,
    amplitude_divide_in_t     *in,
    amplitude_divide_params_t *params,
    amplitude_divide_state_t  *state
) {
    apg_process_info_t info = apg_process_info_default();
    amplitude_divide_process(out, in, params, state, &info);
}
