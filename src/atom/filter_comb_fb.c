#include <atom/dsp_atoms.h>
#include <stdlib.h>

#define CHUNK_LENGTH   512
#define MAX_COMB_DELAY 48000

void filter_comb_fb(
    filter_comb_fb_out_t *out, filter_comb_fb_in_t *in, filter_comb_fb_params_t *params, filter_comb_fb_state_t *state
) {
    if (out->signal == NULL || in->signal == NULL || state == NULL || state->buffer == NULL)
        return;

    int write_pos = state->write_pos;
    int delay     = params->delay_samples;
    if (delay > MAX_COMB_DELAY)
        delay = MAX_COMB_DELAY;

    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        int read_pos = write_pos - delay;
        if (read_pos < 0)
            read_pos += MAX_COMB_DELAY;

        float delayed = state->buffer[read_pos % MAX_COMB_DELAY];
        float output  = in->signal[i] + params->coefficient * delayed;

        out->signal[i]           = output;
        state->buffer[write_pos] = output;
        write_pos                = (write_pos + 1) % MAX_COMB_DELAY;
    }
    state->write_pos = write_pos;
}
