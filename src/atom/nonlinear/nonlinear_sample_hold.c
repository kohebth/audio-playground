#include <atom/dsp_atoms.h>
#include <math.h>
#include <stddef.h>

void nonlinear_sample_hold_process(
    nonlinear_sample_hold_out_t          *out,
    const nonlinear_sample_hold_in_t     *in,
    const nonlinear_sample_hold_params_t *params,
    nonlinear_sample_hold_state_t        *state,
    const apg_process_info_t             *info
) {
    if (out == NULL || in == NULL || params == NULL || state == NULL)
        return;
    if (out->signal == NULL || in->signal == NULL || state == NULL)
        return;

    const uint32_t frames = apg_process_frames_or_default(info);
    float          factor = params->factor;
    if (factor < 1.0f)
        factor = 1.0f;
    float last_val = state->last_val;
    float counter  = state->counter;

    for (uint32_t i = 0; i < frames; ++i) {
        if (counter >= factor) {
            last_val = in->signal[i];
            counter -= factor;
        }
        out->signal[i] = last_val;
        counter += 1.0f;
    }

    state->last_val = last_val;
    state->counter  = counter;
}
