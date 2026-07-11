#include <atom/dsp_atoms.h>
#include <stddef.h>

void filter_differentiate_process(
    filter_differentiate_out_t    *out,
    filter_differentiate_in_t     *in,
    filter_differentiate_params_t *params,
    filter_differentiate_state_t  *state,
    const apg_process_info_t      *info
) {
    if (out == NULL || in == NULL || params == NULL || state == NULL)
        return;
    if (out->signal == NULL || in->signal == NULL || state == NULL)
        return;

    const uint32_t frames = apg_process_frames_or_default(info);
    float          last   = state->prev_sample;
    for (uint32_t i = 0; i < frames; ++i) {
        float x0       = in->signal[i];
        out->signal[i] = x0 - last;
        last           = x0;
    }
    state->prev_sample = last;
}

void filter_differentiate(
    filter_differentiate_out_t    *out,
    filter_differentiate_in_t     *in,
    filter_differentiate_params_t *params,
    filter_differentiate_state_t  *state
) {
    const apg_process_info_t info = apg_process_info_default();
    filter_differentiate_process(out, in, params, state, &info);
}
