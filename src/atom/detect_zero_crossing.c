#include <atom/dsp_atoms.h>
#include <stddef.h>

void detect_zero_crossing_process(
    detect_zero_crossing_out_t    *out,
    detect_zero_crossing_in_t     *in,
    detect_zero_crossing_params_t *params,
    detect_zero_crossing_state_t  *state,
    const apg_process_info_t      *info
) {
    if (out->trigger == NULL || in->signal == NULL || state == NULL)
        return;

    const uint32_t frames = apg_process_frames_or_default(info);
    float          last   = state->prev_sample;
    for (uint32_t i = 0; i < frames; ++i) {
        float current = in->signal[i];
        if ((current > 0 && last <= 0) || (current < 0 && last >= 0)) {
            out->trigger[i] = 1.0f;
        } else {
            out->trigger[i] = 0.0f;
        }
        last = current;
    }
    state->prev_sample = last;
}

void detect_zero_crossing(
    detect_zero_crossing_out_t    *out,
    detect_zero_crossing_in_t     *in,
    detect_zero_crossing_params_t *params,
    detect_zero_crossing_state_t  *state
) {
    const apg_process_info_t info = apg_process_info_default();
    detect_zero_crossing_process(out, in, params, state, &info);
}
