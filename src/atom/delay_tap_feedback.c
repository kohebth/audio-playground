#include <atom/dsp_atoms.h>

#define CHUNK_LENGTH 512

void delay_tap_feedback(delay_tap_feedback_out_t out, delay_tap_feedback_in_t in, delay_tap_feedback_params_t params, void *state) {
    if (out.signal == NULL || in.buffer == NULL) return;

    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        out.signal[i] = in.buffer[in.tap_position] * params.coefficient;
    }
}
