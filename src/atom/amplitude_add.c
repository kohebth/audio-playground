#include <atom/dsp_atoms.h>
#include <stddef.h>

void amplitude_add_process(
    amplitude_add_out_t      *out,
    amplitude_add_in_t       *in,
    amplitude_add_params_t   *params,
    amplitude_add_state_t    *state,
    const apg_process_info_t *info
) {
    (void)params;
    (void)state;
    if (out->signal == NULL || in->signal_a == NULL || in->signal_b == NULL)
        return;

    const uint32_t frames = apg_process_frames_or_default(info);
    for (uint32_t i = 0; i < frames; ++i) {
        out->signal[i] = in->signal_a[i] + in->signal_b[i];
    }
}

void amplitude_add(
    amplitude_add_out_t *out, amplitude_add_in_t *in, amplitude_add_params_t *params, amplitude_add_state_t *state
) {
    apg_process_info_t info = apg_process_info_default();
    amplitude_add_process(out, in, params, state, &info);
}
