#include <atom/dsp_atoms.h>
#include <math.h>
#include <stddef.h>

#define CHUNK_LENGTH 512

void amplitude_clip_soft(
    amplitude_clip_soft_out_t    *out,
    amplitude_clip_soft_in_t     *in,
    amplitude_clip_soft_params_t *params,
    amplitude_clip_soft_state_t  *state
) {
    if (out->signal == NULL || in->signal == NULL)
        return;

    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        float x = in->signal[i] / (params->threshold + 1e-6f);
        if (params->curve == 1) {
            out->signal[i] = params->threshold * tanhf(x);
        } else {
            // Default cubic soft knee
            if (x > 1.0f)
                out->signal[i] = params->threshold * (2.0f / 3.0f);
            else if (x < -1.0f)
                out->signal[i] = -params->threshold * (2.0f / 3.0f);
            else
                out->signal[i] = params->threshold * (x - (x * x * x) / 3.0f);
        }
    }
}
