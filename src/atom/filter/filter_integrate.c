#include <atom/dsp_atoms.h>
#include <stddef.h>

void filter_integrate_process(
    filter_integrate_out_t          *out,
    const filter_integrate_in_t     *in,
    const filter_integrate_params_t *params,
    filter_integrate_state_t        *state,
    const apg_process_context_t     *info
) {
    if (!apg_process_context_valid(info))
        return;
    if (out == NULL || in == NULL || params == NULL || state == NULL)
        return;
    if (out->signal == NULL || in->signal == NULL || state == NULL)
        return;

    const uint32_t frames  = apg_process_context_frames(info);
    float          acc     = state->accumulator;
    const float    leakage = 0.999f;

    for (uint32_t i = 0; i < frames; ++i) {
        acc            = in->signal[i] + leakage * acc;
        out->signal[i] = acc;
    }

    state->accumulator = acc;
}
