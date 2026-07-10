#include <atom/dsp_atoms.h>
#include <apgcore/dsp/dsp_safety.h>
#include <stddef.h>

#define MAX_DELAY_SAMPLES 192000u

void delay_line_process(
    delay_line_out_t         *out,
    delay_line_in_t          *in,
    delay_line_params_t      *params,
    delay_line_state_t       *state,
    const apg_process_info_t *info
) {
    if (out == NULL || in == NULL || out->signal == NULL || in->signal == NULL || params == NULL || state == NULL ||
        state->buffer == NULL)
        return;

    uint32_t write_pos = apg_wrap_index_i64(state->write_pos, MAX_DELAY_SAMPLES);
    int64_t  requested = params->length;
    if (requested < 0)
        requested = 0;
    if (requested >= (int64_t)MAX_DELAY_SAMPLES)
        requested = (int64_t)MAX_DELAY_SAMPLES - 1;
    const uint32_t delay_samples = (uint32_t)requested;

    const uint32_t frames = apg_process_frames_or_default(info);
    for (uint32_t i = 0; i < frames; ++i) {
        const uint32_t read_pos = apg_wrap_index_i64((int64_t)write_pos - (int64_t)delay_samples, MAX_DELAY_SAMPLES);
        out->signal[i]          = state->buffer[read_pos];
        state->buffer[write_pos] = in->signal[i];
        write_pos                = write_pos + 1u == MAX_DELAY_SAMPLES ? 0u : write_pos + 1u;
    }

    state->write_pos = (int)write_pos;
}

void delay_line(delay_line_out_t *out, delay_line_in_t *in, delay_line_params_t *params, delay_line_state_t *state) {
    apg_process_info_t info = apg_process_info_default();
    delay_line_process(out, in, params, state, &info);
}
