#include <apgcore/dsp/dsp_safety.h>
#include <atom/dsp_atoms.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>

void generation_impulse_process(
    generation_impulse_out_t          *out,
    const generation_impulse_in_t     *in,
    const generation_impulse_params_t *params,
    generation_impulse_state_t        *state,
    const apg_process_context_t       *info
) {
    if (!apg_process_context_valid(info))
        return;
    if (out == NULL || in == NULL || params == NULL || state == NULL)
        return;
    (void)in;
    if (out == NULL || out->signal == NULL || params == NULL || state == NULL)
        return;

    const uint32_t frames           = apg_process_context_frames(info);
    const float    sample_rate      = apg_process_context_sample_rate(info);
    const float    interval         = apg_clamp_float(params->interval, 0.0f, (float)INT_MAX / sample_rate);
    int            interval_samples = (int)(interval * sample_rate);
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
