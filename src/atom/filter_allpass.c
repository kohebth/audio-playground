#include <atom/dsp_atoms.h>
#include <stdlib.h>

#define CHUNK_LENGTH      512
#define MAX_ALLPASS_DELAY 48000

void filter_allpass(
    filter_allpass_out_t *out, filter_allpass_in_t *in, filter_allpass_params_t *params, filter_allpass_state_t *state
) {
    if (out->signal == NULL || in->signal == NULL || state == NULL || state->buffer == NULL)
        return;

    int write_pos = state->write_pos;
    int delay     = params->delay_samples;
    if (delay > MAX_ALLPASS_DELAY)
        delay = MAX_ALLPASS_DELAY;
    float g = params->coefficient;

    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        int read_pos = write_pos - delay;
        if (read_pos < 0)
            read_pos += MAX_ALLPASS_DELAY;

        float v_n = state->buffer[read_pos % MAX_ALLPASS_DELAY];
        float y_n = -g * in->signal[i] + v_n;

        out->signal[i]           = y_n;
        state->buffer[write_pos] = in->signal[i] + g * y_n;

        write_pos = (write_pos + 1) % MAX_ALLPASS_DELAY;
    }
    state->write_pos = write_pos;
}
