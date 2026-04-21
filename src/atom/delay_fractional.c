#include <atom/dsp_atoms.h>
#include <stdlib.h>
#include <math.h>

#define CHUNK_LENGTH 512
#define MAX_DELAY_SAMPLES 192000

void delay_fractional(delay_fractional_out_t out, delay_fractional_in_t in, delay_fractional_params_t params, delay_fractional_state_t *state) {
    if (out.signal == NULL || in.signal == NULL || state == NULL || state->buffer == NULL) return;

    int write_pos = state->write_pos;
    float delay = params.delay_samples;
    if (delay > MAX_DELAY_SAMPLES - 1) delay = MAX_DELAY_SAMPLES - 1;
    if (delay < 0) delay = 0;

    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        float read_pos = (float)write_pos - delay;
        if (read_pos < 0) read_pos += MAX_DELAY_SAMPLES;

        uint32_t idx_a = (uint32_t)floorf(read_pos) % MAX_DELAY_SAMPLES;
        uint32_t idx_b = (idx_a + 1) % MAX_DELAY_SAMPLES;
        float frac = read_pos - floorf(read_pos);

        out.signal[i] = state->buffer[idx_a] * (1.0f - frac) + state->buffer[idx_b] * frac;
        state->buffer[write_pos] = in.signal[i];

        write_pos = (write_pos + 1) % MAX_DELAY_SAMPLES;
    }

    state->write_pos = write_pos;
}
