#include <atom/dsp_atoms.h>
#include <stddef.h>

#define CHUNK_LENGTH 512

void amplitude_latch(
    amplitude_latch_out_t    *out,
    amplitude_latch_in_t     *in,
    amplitude_latch_params_t *params,
    amplitude_latch_state_t  *state
) {
    if (out->signal == NULL || in->signal == NULL || in->gate == NULL || state == NULL)
        return;

    float latched_value = state->latched_value;
    int prev_gate = state->prev_gate;
    float threshold = params->threshold;
    if (threshold <= 0.0f) threshold = 0.5f;

    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        int current_gate = (in->gate[i] >= threshold) ? 1 : 0;
        
        // Positive edge trigger OR waiting for first valid pitch OR significant signal change
        if (current_gate) {
            float diff = (in->signal[i] > latched_value) ? (in->signal[i] - latched_value) : (latched_value - in->signal[i]);
            if (!prev_gate || latched_value <= 0.0f || (diff > 1.0f && in->signal[i] > 0.0f)) {
                if (in->signal[i] > 0.0f) {
                    latched_value = in->signal[i];
                } else if (!prev_gate) {
                    latched_value = 0.0f;
                }
            }
        } 
        
        out->signal[i] = latched_value;
        prev_gate = current_gate;
    }

    state->latched_value = latched_value;
    state->prev_gate = prev_gate;
}
