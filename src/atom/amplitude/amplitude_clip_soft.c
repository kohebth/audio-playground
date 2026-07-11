#include <atom/dsp_atoms.h>
#include <math.h>
#include <stddef.h>

void amplitude_clip_soft_process(
    amplitude_clip_soft_out_t    *out,
    amplitude_clip_soft_in_t     *in,
    amplitude_clip_soft_params_t *params,
    amplitude_clip_soft_state_t  *state,
    const apg_process_info_t     *info
) {
    if (out == NULL || in == NULL || params == NULL || state == NULL)
        return;
    (void)state;
    if (out->signal == NULL || in->signal == NULL || params == NULL)
        return;

    const uint32_t frames = apg_process_frames_or_default(info);
    for (uint32_t i = 0; i < frames; ++i) {
        float x = in->signal[i] / (params->threshold + 1e-6f);
        if (params->curve == 1) {
            out->signal[i] = params->threshold * tanhf(x);
        } else {
            if (x > 1.0f)
                out->signal[i] = params->threshold * (2.0f / 3.0f);
            else if (x < -1.0f)
                out->signal[i] = -params->threshold * (2.0f / 3.0f);
            else
                out->signal[i] = params->threshold * (x - (x * x * x) / 3.0f);
        }
    }
}

void amplitude_clip_soft(
    amplitude_clip_soft_out_t    *out,
    amplitude_clip_soft_in_t     *in,
    amplitude_clip_soft_params_t *params,
    amplitude_clip_soft_state_t  *state
) {
    apg_process_info_t info = apg_process_info_default();
    amplitude_clip_soft_process(out, in, params, state, &info);
}
