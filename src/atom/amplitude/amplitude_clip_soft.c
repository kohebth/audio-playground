#include <atom/dsp_atoms.h>
#include <math.h>
#include <stddef.h>

void amplitude_clip_soft_process(
    amplitude_clip_soft_out_t          *out,
    const amplitude_clip_soft_in_t     *in,
    const amplitude_clip_soft_params_t *params,
    amplitude_clip_soft_state_t        *state,
    const apg_process_context_t        *info
) {
    if (!apg_process_context_valid(info))
        return;
    if (out == NULL || in == NULL || params == NULL || state == NULL)
        return;
    (void)state;
    if (out->signal == NULL || in->signal == NULL || params == NULL)
        return;

    const uint32_t frames = apg_process_context_frames(info);
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
