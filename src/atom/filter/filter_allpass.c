#include <apgcore/dsp/dsp_safety.h>
#include <atom/dsp_atoms.h>
#include <stddef.h>

#define MAX_ALLPASS_DELAY 48000u

void filter_allpass_process(
    filter_allpass_out_t          *out,
    const filter_allpass_in_t     *in,
    const filter_allpass_params_t *params,
    filter_allpass_state_t        *state,
    const apg_process_context_t   *info
) {
    if (!apg_process_context_valid(info))
        return;
    if (out == NULL || in == NULL || params == NULL || state == NULL)
        return;
    if (out == NULL || in == NULL || params == NULL || state == NULL || out->signal == NULL || in->signal == NULL ||
        state->buffer == NULL)
        return;

    const uint32_t capacity  = state->buffer_len > 0u ? state->buffer_len : MAX_ALLPASS_DELAY;
    const uint32_t frames    = apg_process_context_frames(info);
    uint32_t       write_pos = apg_wrap_index_i64(state->write_pos, capacity);
    int64_t        delay     = params->delay_samples;
    if (delay < 1)
        delay = 1;
    if (delay > (int64_t)capacity)
        delay = capacity;
    const float coefficient =
        isfinite(params->coefficient) ? apg_clamp_float(params->coefficient, -0.999f, 0.999f) : 0.0f;

    for (uint32_t i = 0; i < frames; ++i) {
        const uint32_t read_pos = apg_wrap_index_i64((int64_t)write_pos - delay, capacity);
        float          delayed  = state->buffer[read_pos];
        if (!isfinite(delayed)) {
            delayed                 = 0.0f;
            state->buffer[read_pos] = 0.0f;
        }

        const float input  = isfinite(in->signal[i]) ? in->signal[i] : 0.0f;
        float       output = -coefficient * input + delayed;
        float       next   = input + coefficient * output;
        if (!isfinite(output) || !isfinite(next)) {
            output = 0.0f;
            next   = 0.0f;
        }

        out->signal[i]           = output;
        state->buffer[write_pos] = apg_denormal_kill(next);

        write_pos = write_pos + 1u == capacity ? 0u : write_pos + 1u;
    }
    state->write_pos = (int)write_pos;
}
