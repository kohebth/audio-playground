#include <atom/dsp_atoms.h>
#include <stddef.h>
#include <stdint.h>

void generation_impulse_process(
    generation_impulse_out_t    *out,
    generation_impulse_in_t     *in,
    generation_impulse_params_t *params,
    generation_impulse_state_t  *state,
    const apg_process_info_t    *info
) {
    if (out->signal == NULL || state == NULL)
        return;

    const uint32_t frames           = apg_process_frames_or_default(info);
    int            interval_samples = (int)(params->interval * params->sample_rate);
    if (interval_samples < 1)
        interval_samples = 1;

    for (uint32_t i = 0; i < frames; ++i) {
        if (state->counter <= 0) {
            out->signal[i] = 1.0f;
            state->counter = interval_samples - 1;
        } else {
            out->signal[i] = 0.0f;
            state->counter--;
        }
    }
}

void generation_impulse(
    generation_impulse_out_t    *out,
    generation_impulse_in_t     *in,
    generation_impulse_params_t *params,
    generation_impulse_state_t  *state
) {
    const apg_process_info_t info = apg_process_info_default();
    generation_impulse_process(out, in, params, state, &info);
}
