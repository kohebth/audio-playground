#include <apgcore/dsp/dsp_safety.h>
#include <atom/dsp_atoms.h>
#include <stddef.h>

void delay_line_process(
    delay_line_out_t            *out,
    const delay_line_in_t       *in,
    const delay_line_params_t   *params,
    delay_line_state_t          *state,
    const apg_process_context_t *info
) {
    if (!apg_process_context_valid(info))
        return;
    if (out == NULL || in == NULL || params == NULL || state == NULL)
        return;
    if (out == NULL || in == NULL || out->signal == NULL || in->signal == NULL || params == NULL || state == NULL ||
        state->buffer == NULL || state->buffer_len == 0u)
        return;

    const uint32_t capacity  = state->buffer_len;
    uint32_t       write_pos = apg_wrap_index_i64(state->write_pos, capacity);
    int64_t        requested = params->length;
    if (requested < 0)
        requested = 0;
    if (requested >= (int64_t)capacity)
        requested = (int64_t)capacity - 1;
    const uint32_t delay_samples = (uint32_t)requested;

    const uint32_t frames = apg_process_context_frames(info);
    for (uint32_t i = 0; i < frames; ++i) {
        const uint32_t read_pos  = apg_wrap_index_i64((int64_t)write_pos - (int64_t)delay_samples, capacity);
        out->signal[i]           = state->buffer[read_pos];
        state->buffer[write_pos] = in->signal[i];
        write_pos                = write_pos + 1u == capacity ? 0u : write_pos + 1u;
    }

    state->write_pos = (int)write_pos;
}
