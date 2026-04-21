#include <atom/dsp_atoms.h>

#define CHUNK_LENGTH 512

void detect_zero_crossing(detect_zero_crossing_out_t out, detect_zero_crossing_in_t in, void *params, detect_zero_crossing_state_t *state) {
    if (out.trigger == NULL || in.signal == NULL || state == NULL) return;

    float last = state->prev_sample;
    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        float current = in.signal[i];
        if ((current > 0 && last <= 0) || (current < 0 && last >= 0)) {
            out.trigger[i] = 1.0f;
        } else {
            out.trigger[i] = 0.0f;
        }
        last = current;
    }
    state->prev_sample = last;
}
