#include <atom/dsp_atoms.h>
#include <stddef.h>

void amplitude_add_process(
    amplitude_add_out_t          *out,
    const amplitude_add_in_t     *in,
    const amplitude_add_params_t *params,
    amplitude_add_state_t        *state,
    const apg_process_context_t  *info
) {
    if (!apg_process_context_valid(info))
        return;
    if (out == NULL || in == NULL || params == NULL || state == NULL)
        return;
    (void)params;
    (void)state;
    if (out->signal == NULL || in->signal_a == NULL || in->signal_b == NULL)
        return;

    const uint32_t frames = apg_process_context_frames(info);
    for (uint32_t i = 0; i < frames; ++i) {
        out->signal[i] = in->signal_a[i] + in->signal_b[i];
    }
}
