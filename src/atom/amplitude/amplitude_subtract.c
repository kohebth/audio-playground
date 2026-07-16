#include <atom/dsp_atoms.h>
#include <stddef.h>

void amplitude_subtract_process(
    amplitude_subtract_out_t          *out,
    const amplitude_subtract_in_t     *in,
    const amplitude_subtract_params_t *params,
    amplitude_subtract_state_t        *state,
    const apg_process_info_t          *info
) {
    if (out == NULL || in == NULL || params == NULL || state == NULL)
        return;
    (void)params;
    (void)state;
    if (out->signal == NULL || in->signal_a == NULL || in->signal_b == NULL)
        return;

    const uint32_t frames = apg_process_frames_or_default(info);
    for (uint32_t i = 0; i < frames; ++i) {
        out->signal[i] = in->signal_a[i] - in->signal_b[i];
    }
}
