#include <atom/dsp_atoms.h>
#include <stddef.h>

void detect_slope_process(
    detect_slope_out_t          *out,
    const detect_slope_in_t     *in,
    const detect_slope_params_t *params,
    detect_slope_state_t        *state,
    const apg_process_info_t    *info
) {
    if (out == NULL || in == NULL || params == NULL || state == NULL)
        return;
    if (out->slope == NULL || in->signal == NULL || state == NULL)
        return;

    const uint32_t frames = apg_process_frames_or_default(info);
    float          last   = state->prev_sample;
    for (uint32_t i = 0; i < frames; ++i) {
        out->slope[i] = in->signal[i] - last;
        last          = in->signal[i];
    }
    state->prev_sample = last;
}
