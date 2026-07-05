#include <atom/dsp_atoms.h>
#include <stddef.h>

void delay_unit_process(
    delay_unit_out_t         *out,
    delay_unit_in_t          *in,
    delay_unit_params_t      *params,
    delay_unit_state_t       *state,
    const apg_process_info_t *info
) {
    (void)params;
    if (out->signal == NULL || in->signal == NULL || state == NULL)
        return;

    float          last   = state->prev_sample;
    const uint32_t frames = apg_process_frames_or_default(info);
    for (uint32_t i = 0; i < frames; ++i) {
        float current  = in->signal[i];
        out->signal[i] = last;
        last           = current;
    }
    state->prev_sample = last;
}

void delay_unit(delay_unit_out_t *out, delay_unit_in_t *in, delay_unit_params_t *params, delay_unit_state_t *state) {
    apg_process_info_t info = apg_process_info_default();
    delay_unit_process(out, in, params, state, &info);
}
