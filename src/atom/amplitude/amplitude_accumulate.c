#include <atom/dsp_atoms.h>
#include <stddef.h>

void amplitude_accumulate_process(
    amplitude_accumulate_out_t    *out,
    amplitude_accumulate_in_t     *in,
    amplitude_accumulate_params_t *params,
    amplitude_accumulate_state_t  *state,
    const apg_process_info_t      *info
) {
    if (out == NULL || in == NULL || params == NULL || state == NULL)
        return;
    if (out->signal == NULL || in->signal == NULL || state == NULL)
        return;

    const uint32_t frames = apg_process_frames_or_default(info);
    float          sum    = state->accumulator;
    for (uint32_t i = 0; i < frames; ++i) {
        sum += in->signal[i];
        out->signal[i] = sum;
    }
    state->accumulator = sum;
}

void amplitude_accumulate(
    amplitude_accumulate_out_t    *out,
    amplitude_accumulate_in_t     *in,
    amplitude_accumulate_params_t *params,
    amplitude_accumulate_state_t  *state
) {
    const apg_process_info_t info = apg_process_info_default();
    amplitude_accumulate_process(out, in, params, state, &info);
}
