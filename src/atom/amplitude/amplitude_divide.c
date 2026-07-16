#include <atom/dsp_atoms.h>
#include <math.h>
#include <stddef.h>

void amplitude_divide_process(
    amplitude_divide_out_t          *out,
    const amplitude_divide_in_t     *in,
    const amplitude_divide_params_t *params,
    amplitude_divide_state_t        *state,
    const apg_process_context_t     *info
) {
    if (!apg_process_context_valid(info))
        return;
    if (out == NULL || in == NULL || params == NULL || state == NULL)
        return;
    (void)state;
    if (out->signal == NULL || in->numerator == NULL || in->denominator == NULL || params == NULL)
        return;

    const uint32_t frames = apg_process_context_frames(info);
    for (uint32_t i = 0; i < frames; ++i) {
        if (fabsf(in->denominator[i]) > params->epsilon) {
            out->signal[i] = in->numerator[i] / in->denominator[i];
        } else {
            out->signal[i] = 0.0f;
        }
    }
}
