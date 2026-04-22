#include <atom/dsp_atoms.h>
#include <math.h>
#include <stddef.h>

#define CHUNK_LENGTH 512

void amplitude_normalize(
    amplitude_normalize_out_t    *out,
    amplitude_normalize_in_t     *in,
    amplitude_normalize_params_t *params,
    amplitude_normalize_state_t  *state
) {
    if (out->signal == NULL || in->signal == NULL || state == NULL)
        return;

    float peak = state->running_peak;
    if (params->mode == NORMALIZE_PEAK) {
        for (int i = 0; i < CHUNK_LENGTH; ++i) {
            float abs_val = fabsf(in->signal[i]);
            if (abs_val > peak)
                peak = abs_val;
        }
    } else {
        float sum_sq = 0.0f;
        for (int i = 0; i < CHUNK_LENGTH; ++i) {
            sum_sq += in->signal[i] * in->signal[i];
        }
        peak = sqrtf(sum_sq / CHUNK_LENGTH);
    }

    float gain = 1.0f;
    if (peak > 1e-6f) {
        gain = params->target_level / peak;
    }

    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        out->signal[i] = in->signal[i] * gain;
    }

    state->running_peak = peak;
}
