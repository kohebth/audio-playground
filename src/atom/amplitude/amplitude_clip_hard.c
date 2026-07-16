#include <atom/dsp_atoms.h>
#include <stddef.h>

void amplitude_clip_hard_process(
    amplitude_clip_hard_out_t          *out,
    const amplitude_clip_hard_in_t     *in,
    const amplitude_clip_hard_params_t *params,
    amplitude_clip_hard_state_t        *state,
    const apg_process_info_t           *info
) {
    if (out == NULL || in == NULL || params == NULL || state == NULL)
        return;
    if (out->signal == NULL || in->signal == NULL)
        return;

    const uint32_t frames = apg_process_frames_or_default(info);

    for (uint32_t i = 0; i < frames; ++i) {
        float s = in->signal[i];
        if (s > params->threshold)
            s = params->threshold;
        else if (s < -params->threshold)
            s = -params->threshold;
        out->signal[i] = s;
    }
}
