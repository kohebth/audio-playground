#include <atom/dsp_atoms.h>
#include <stddef.h>

void filter_differentiate_process(
    filter_differentiate_out_t          *out,
    const filter_differentiate_in_t     *in,
    const filter_differentiate_params_t *params,
    filter_differentiate_state_t        *state,
    const apg_process_context_t         *info
) {
    if (!apg_process_context_valid(info))
        return;
    if (out == NULL || in == NULL || params == NULL || state == NULL)
        return;
    if (out->signal == NULL || in->signal == NULL || state == NULL)
        return;

    const uint32_t frames = apg_process_context_frames(info);
    float          last   = state->prev_sample;
    for (uint32_t i = 0; i < frames; ++i) {
        float x0       = in->signal[i];
        out->signal[i] = x0 - last;
        last           = x0;
    }
    state->prev_sample = last;
}
