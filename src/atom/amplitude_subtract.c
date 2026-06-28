#include <atom/dsp_atoms.h>
#include <stddef.h>

void amplitude_subtract_process(
    amplitude_subtract_out_t    *out,
    amplitude_subtract_in_t     *in,
    amplitude_subtract_params_t *params,
    amplitude_subtract_state_t  *state,
    const apg_process_info_t    *info
) {
    (void)params;
    (void)state;
    if (out->signal == NULL || in->signal_a == NULL || in->signal_b == NULL)
        return;

    const uint32_t frames = apg_process_frames_or_default(info);
    for (uint32_t i = 0; i < frames; ++i) {
        out->signal[i] = in->signal_a[i] - in->signal_b[i];
    }
}

void amplitude_subtract(
    amplitude_subtract_out_t    *out,
    amplitude_subtract_in_t     *in,
    amplitude_subtract_params_t *params,
    amplitude_subtract_state_t  *state
) {
    apg_process_info_t info = apg_process_info_default();
    amplitude_subtract_process(out, in, params, state, &info);
}
