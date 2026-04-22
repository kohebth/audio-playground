#include <atom/dsp_atoms.h>
#include <stddef.h>

#define CHUNK_LENGTH 512

void delay_tap_feedforward(
    delay_tap_feedforward_out_t    *out,
    delay_tap_feedforward_in_t     *in,
    delay_tap_feedforward_params_t *params,
    delay_tap_feedforward_state_t  *state
) {
    if (out->signal == NULL || in->buffer == NULL)
        return;

    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        out->signal[i] = in->buffer[in->tap_position] * params->coefficient;
    }
}
