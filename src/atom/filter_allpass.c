#include <atom/dsp_atoms.h>
#include <stdlib.h>

#define MAX_ALLPASS_DELAY 48000

void filter_allpass_process(
    filter_allpass_out_t     *out,
    filter_allpass_in_t      *in,
    filter_allpass_params_t  *params,
    filter_allpass_state_t   *state,
    const apg_process_info_t *info
) {
    if (out->signal == NULL || in->signal == NULL || state == NULL || state->buffer == NULL)
        return;

    const uint32_t frames    = apg_process_frames_or_default(info);
    int            write_pos = state->write_pos;
    int            delay     = params->delay_samples;
    if (delay > MAX_ALLPASS_DELAY)
        delay = MAX_ALLPASS_DELAY;
    float g = params->coefficient;

    for (uint32_t i = 0; i < frames; ++i) {
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

void filter_allpass(
    filter_allpass_out_t *out, filter_allpass_in_t *in, filter_allpass_params_t *params, filter_allpass_state_t *state
) {
    const apg_process_info_t info = apg_process_info_default();
    filter_allpass_process(out, in, params, state, &info);
}
