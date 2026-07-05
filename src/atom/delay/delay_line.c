#include <atom/dsp_atoms.h>
#include <stddef.h>

#define MAX_DELAY_SAMPLES 192000 // 4 seconds at 48kHz

void delay_line_process(
    delay_line_out_t         *out,
    delay_line_in_t          *in,
    delay_line_params_t      *params,
    delay_line_state_t       *state,
    const apg_process_info_t *info
) {
    if (out->signal == NULL || in->signal == NULL || params == NULL || state == NULL || state->buffer == NULL) {
        return;
    }

    int write_pos     = state->write_pos;
    int delay_samples = params->length;
    if (delay_samples < 0) {
        delay_samples = 0;
    }
    if (delay_samples > MAX_DELAY_SAMPLES) {
        delay_samples = MAX_DELAY_SAMPLES;
    }

    const uint32_t frames = apg_process_frames_or_default(info);
    for (uint32_t i = 0; i < frames; ++i) {
        int read_pos = write_pos - delay_samples;
        if (read_pos < 0) {
            read_pos += MAX_DELAY_SAMPLES;
        }

        out->signal[i]           = state->buffer[read_pos % MAX_DELAY_SAMPLES];
        state->buffer[write_pos] = in->signal[i];

        write_pos = (write_pos + 1) % MAX_DELAY_SAMPLES;
    }

    state->write_pos = write_pos;
}

void delay_line(delay_line_out_t *out, delay_line_in_t *in, delay_line_params_t *params, delay_line_state_t *state) {
    apg_process_info_t info = apg_process_info_default();
    delay_line_process(out, in, params, state, &info);
}
