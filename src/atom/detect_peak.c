#include <atom/dsp_atoms.h>
#include <math.h>
#include <stddef.h>

#define CHUNK_LENGTH 512

void detect_peak(
    detect_peak_out_t *out, detect_peak_in_t *in, detect_peak_params_t *params, detect_peak_state_t *state
) {
    if (out->level == NULL || in->signal == NULL || state == NULL)
        return;

    float peak          = state->prev_peak;
    float attack_coeff  = expf(-1.0f / (params->attack * params->sample_rate + 1.0f));
    float release_coeff = expf(-1.0f / (params->release * params->sample_rate + 1.0f));

    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        float abs_x = fabsf(in->signal[i]);
        if (abs_x > peak) {
            peak = abs_x + attack_coeff * (peak - abs_x);
        } else {
            peak = abs_x + release_coeff * (peak - abs_x);
        }
        out->level[i] = peak;
    }
    state->prev_peak = peak;
}
