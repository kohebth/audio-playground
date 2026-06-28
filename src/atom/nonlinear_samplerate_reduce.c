#include <atom/dsp_atoms.h>
#include <math.h>
#include <stddef.h>

void nonlinear_samplerate_reduce_process(
    nonlinear_samplerate_reduce_out_t    *out,
    nonlinear_samplerate_reduce_in_t     *in,
    nonlinear_samplerate_reduce_params_t *params,
    nonlinear_samplerate_reduce_state_t  *state,
    const apg_process_info_t             *info
) {
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

void nonlinear_samplerate_reduce(
    nonlinear_samplerate_reduce_out_t    *out,
    nonlinear_samplerate_reduce_in_t     *in,
    nonlinear_samplerate_reduce_params_t *params,
    nonlinear_samplerate_reduce_state_t  *state
) {
    const apg_process_info_t info = apg_process_info_default();
    nonlinear_samplerate_reduce_process(out, in, params, state, &info);
}
