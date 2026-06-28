#include <atom/dsp_atoms.h>
#include <stdlib.h>

#define MAX_COMB_DELAY 48000

void filter_comb_ff_process(
    filter_comb_ff_out_t     *out,
    filter_comb_ff_in_t      *in,
    filter_comb_ff_params_t  *params,
    filter_comb_ff_state_t   *state,
    const apg_process_info_t *info
) {
    if (out->signal == NULL || in->signal == NULL || state == NULL || state->buffer == NULL)
        return;

    const uint32_t frames    = apg_process_frames_or_default(info);
    int            write_pos = state->write_pos;
    int            delay     = params->delay_samples;
    if (delay > MAX_COMB_DELAY)
        delay = MAX_COMB_DELAY;

    for (uint32_t i = 0; i < frames; ++i) {
        int read_pos = write_pos - delay;
        if (read_pos < 0)
            read_pos += MAX_COMB_DELAY;

        float delayed  = state->buffer[read_pos % MAX_COMB_DELAY];
        out->signal[i] = in->signal[i] + params->coefficient * delayed;

        state->buffer[write_pos] = in->signal[i];
        write_pos                = (write_pos + 1) % MAX_COMB_DELAY;
    }
    state->write_pos = write_pos;
}

void filter_comb_ff(
    filter_comb_ff_out_t *out, filter_comb_ff_in_t *in, filter_comb_ff_params_t *params, filter_comb_ff_state_t *state
) {
    const apg_process_info_t info = apg_process_info_default();
    filter_comb_ff_process(out, in, params, state, &info);
}
