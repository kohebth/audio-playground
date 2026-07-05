#include <atom/dsp_atoms.h>
#include <math.h>
#include <stdlib.h>

#define MAX_FM_DELAY 4096

void modulation_frequency_process(
    modulation_frequency_out_t    *out,
    modulation_frequency_in_t     *in,
    modulation_frequency_params_t *params,
    modulation_frequency_state_t  *state,
    const apg_process_info_t      *info
) {
    if (out->signal == NULL || in->signal == NULL || in->modulator == NULL || state == NULL || state->buffer == NULL)
        return;

    const uint32_t frames        = apg_process_frames_or_default(info);
    int            write_pos     = state->write_pos;
    float          current_delay = state->current_delay;

    for (uint32_t i = 0; i < frames; ++i) {
        current_delay += in->modulator[i] * params->depth;
        if (current_delay < 0)
            current_delay = 0;
        if (current_delay > MAX_FM_DELAY - 1)
            current_delay = MAX_FM_DELAY - 1;

        float read_pos = (float)write_pos - current_delay;
        if (read_pos < 0)
            read_pos += MAX_FM_DELAY;

        uint32_t idx_a = (uint32_t)floorf(read_pos) % MAX_FM_DELAY;
        uint32_t idx_b = (idx_a + 1) % MAX_FM_DELAY;
        float    frac  = read_pos - floorf(read_pos);

        out->signal[i]           = state->buffer[idx_a] * (1.0f - frac) + state->buffer[idx_b] * frac;
        state->buffer[write_pos] = in->signal[i];
        write_pos                = (write_pos + 1) % MAX_FM_DELAY;
    }

    state->write_pos     = write_pos;
    state->current_delay = current_delay;
}

void modulation_frequency(
    modulation_frequency_out_t    *out,
    modulation_frequency_in_t     *in,
    modulation_frequency_params_t *params,
    modulation_frequency_state_t  *state
) {
    const apg_process_info_t info = apg_process_info_default();
    modulation_frequency_process(out, in, params, state, &info);
}
