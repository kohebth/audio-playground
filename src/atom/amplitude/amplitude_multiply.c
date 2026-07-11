#include <atom/dsp_atoms.h>
#include <stddef.h>

void amplitude_multiply_process(
    amplitude_multiply_out_t    *out,
    amplitude_multiply_in_t     *in,
    amplitude_multiply_params_t *params,
    amplitude_multiply_state_t  *state,
    const apg_process_info_t    *info
) {
    if (out == NULL || in == NULL || params == NULL || state == NULL)
        return;
    (void)params;
    (void)state;
    if (out->signal == NULL || in->signal_a == NULL || in->signal_b == NULL)
        return;

    const uint32_t frames = apg_process_frames_or_default(info);
    for (uint32_t i = 0; i < frames; ++i) {
        out->signal[i] = in->signal_a[i] * in->signal_b[i];
    }
}

void amplitude_multiply(
    amplitude_multiply_out_t    *out,
    amplitude_multiply_in_t     *in,
    amplitude_multiply_params_t *params,
    amplitude_multiply_state_t  *state
) {
    apg_process_info_t info = apg_process_info_default();
    amplitude_multiply_process(out, in, params, state, &info);
}
