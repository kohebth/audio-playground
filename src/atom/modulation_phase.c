#include <atom/dsp_atoms.h>
#include <math.h>
#include <stdlib.h>

#define MAX_MOD_DELAY 4096

void modulation_phase_process(
    modulation_phase_out_t    *out,
    modulation_phase_in_t     *in,
    modulation_phase_params_t *params,
    modulation_phase_state_t  *state,
    const apg_process_info_t  *info
) {
    if (out->signal == NULL || in->signal == NULL || in->modulator == NULL || state == NULL || state->buffer == NULL)
        return;

    const uint32_t frames    = apg_process_frames_or_default(info);
    int            write_pos = state->write_pos;

    for (uint32_t i = 0; i < frames; ++i) {
        float delay_samples = params->depth * (1.0f + in->modulator[i] * 0.5f);
        if (delay_samples < 0)
            delay_samples = 0;
        if (delay_samples > MAX_MOD_DELAY - 1)
            delay_samples = MAX_MOD_DELAY - 1;

        float read_pos = (float)write_pos - delay_samples;
        if (read_pos < 0)
            read_pos += MAX_MOD_DELAY;

        uint32_t idx1 = (uint32_t)floorf(read_pos) % MAX_MOD_DELAY;
        uint32_t idx0 = (idx1 > 0) ? idx1 - 1 : MAX_MOD_DELAY - 1;
        uint32_t idx2 = (idx1 + 1) % MAX_MOD_DELAY;
        uint32_t idx3 = (idx2 + 1) % MAX_MOD_DELAY;

        float frac  = read_pos - floorf(read_pos);
        float frac2 = frac * frac;
        float frac3 = frac2 * frac;

        float v0 = state->buffer[idx0];
        float v1 = state->buffer[idx1];
        float v2 = state->buffer[idx2];
        float v3 = state->buffer[idx3];

        float a = -0.5f * v0 + 1.5f * v1 - 1.5f * v2 + 0.5f * v3;
        float b = v0 - 2.5f * v1 + 2.0f * v2 - 0.5f * v3;
        float c = -0.5f * v0 + 0.5f * v2;
        float d = v1;

        out->signal[i]           = a * frac3 + b * frac2 + c * frac + d;
        state->buffer[write_pos] = in->signal[i];
        write_pos                = (write_pos + 1) % MAX_MOD_DELAY;
    }

    state->write_pos = write_pos;
}

void modulation_phase(
    modulation_phase_out_t    *out,
    modulation_phase_in_t     *in,
    modulation_phase_params_t *params,
    modulation_phase_state_t  *state
) {
    const apg_process_info_t info = apg_process_info_default();
    modulation_phase_process(out, in, params, state, &info);
}
