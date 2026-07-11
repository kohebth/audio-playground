#include <atom/dsp_atoms.h>
#include <stddef.h>

void interpolation_linear_process(
    interpolation_linear_out_t    *out,
    interpolation_linear_in_t     *in,
    interpolation_linear_params_t *params,
    interpolation_linear_state_t  *state,
    const apg_process_info_t      *info
) {
    if (out == NULL || in == NULL || params == NULL || state == NULL)
        return;
    if (out->signal == NULL || in->signal_a == NULL || in->signal_b == NULL || in->t == NULL)
        return;

    const uint32_t frames = apg_process_frames_or_default(info);

    for (uint32_t i = 0; i < frames; ++i) {
        out->signal[i] = in->signal_a[i] * (1.0f - in->t[i]) + in->signal_b[i] * in->t[i];
    }
}

void interpolation_linear(
    interpolation_linear_out_t    *out,
    interpolation_linear_in_t     *in,
    interpolation_linear_params_t *params,
    interpolation_linear_state_t  *state
) {
    const apg_process_info_t info = apg_process_info_default();
    interpolation_linear_process(out, in, params, state, &info);
}
