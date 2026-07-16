#include <atom/dsp_atoms.h>
#include <stddef.h>

void amplitude_accumulate_process(
    amplitude_accumulate_out_t          *out,
    const amplitude_accumulate_in_t     *in,
    const amplitude_accumulate_params_t *params,
    amplitude_accumulate_state_t        *state,
    const apg_process_context_t         *info
) {
    if (!apg_process_context_valid(info))
        return;
    if (out == NULL || in == NULL || params == NULL || state == NULL)
        return;
    if (out->signal == NULL || in->signal == NULL || state == NULL)
        return;

    const uint32_t frames = apg_process_context_frames(info);
    float          sum    = state->accumulator;
    for (uint32_t i = 0; i < frames; ++i) {
        sum += in->signal[i];
        out->signal[i] = sum;
    }
    state->accumulator = sum;
}
