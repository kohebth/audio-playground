#include <atom/dsp_atoms.h>
#include <stdlib.h>
#include <math.h>

#define CHUNK_LENGTH 512
#define MAX_FM_DELAY 4096

void modulation_frequency(modulation_frequency_out_t out, modulation_frequency_in_t in, modulation_frequency_params_t params, modulation_frequency_state_t *state) {
    if (out.signal == NULL || in.signal == NULL || in.modulator == NULL || state == NULL || state->buffer == NULL) return;

    int write_pos = state->write_pos;
    float current_delay = state->current_delay;

    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        current_delay += in.modulator[i] * params.depth;
        if (current_delay < 0) current_delay = 0;
        if (current_delay > MAX_FM_DELAY - 1) current_delay = MAX_FM_DELAY - 1;

        float read_pos = (float)write_pos - current_delay;
        if (read_pos < 0) read_pos += MAX_FM_DELAY;

        uint32_t idx_a = (uint32_t)floorf(read_pos) % MAX_FM_DELAY;
        uint32_t idx_b = (idx_a + 1) % MAX_FM_DELAY;
        float frac = read_pos - floorf(read_pos);

        out.signal[i] = state->buffer[idx_a] * (1.0f - frac) + state->buffer[idx_b] * frac;
        state->buffer[write_pos] = in.signal[i];
        write_pos = (write_pos + 1) % MAX_FM_DELAY;
    }

    state->write_pos = write_pos;
    state->current_delay = current_delay;
}
