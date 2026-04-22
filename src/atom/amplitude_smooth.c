#include <atom/dsp_atoms.h>
#include <math.h>
#include <stddef.h>

#define CHUNK_LENGTH 512

void amplitude_smooth(
    amplitude_smooth_out_t    *out,
    amplitude_smooth_in_t     *in,
    amplitude_smooth_params_t *params,
    amplitude_smooth_state_t  *state
) {
    if (out->signal == NULL || in->signal == NULL || state == NULL)
        return;

    float alpha_att = 1.0f - expf(-1.0f / (params->attack * params->sample_rate + 1.0f));
    float alpha_rel = 1.0f - expf(-1.0f / (params->release * params->sample_rate + 1.0f));
    float last_out  = state->prev_value;

    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        float input = in->signal[i];
        float alpha = (input > last_out) ? alpha_att : alpha_rel;
        last_out += alpha * (input - last_out);
        out->signal[i] = last_out;
    }

    state->prev_value = last_out;
}
