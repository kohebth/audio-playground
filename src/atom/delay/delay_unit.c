#include <atom/dsp_atoms.h>
#include <stddef.h>

void delay_unit_process(
    delay_unit_out_t            *out,
    const delay_unit_in_t       *in,
    const delay_unit_params_t   *params,
    delay_unit_state_t          *state,
    const apg_process_context_t *info
) {
    if (!apg_process_context_valid(info))
        return;
    if (out == NULL || in == NULL || params == NULL || state == NULL)
        return;
    (void)params;
    if (out->signal == NULL || in->signal == NULL || state == NULL)
        return;

    float          last   = state->prev_sample;
    const uint32_t frames = apg_process_context_frames(info);
    for (uint32_t i = 0; i < frames; ++i) {
        float current  = in->signal[i];
        out->signal[i] = last;
        last           = current;
    }
    state->prev_sample = last;
}
