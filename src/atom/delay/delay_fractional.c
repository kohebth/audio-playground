#include <apgcore/dsp/dsp_safety.h>
#include <atom/dsp_atoms.h>
#include <math.h>
#include <stddef.h>

#define MAX_DELAY_SAMPLES 192000u

void delay_fractional_process(
    delay_fractional_out_t    *out,
    delay_fractional_in_t     *in,
    delay_fractional_params_t *params,
    delay_fractional_state_t  *state,
    const apg_process_info_t  *info
) {
    if (out == NULL || in == NULL || params == NULL || state == NULL)
        return;
    if (out == NULL || in == NULL || out->signal == NULL || in->signal == NULL || params == NULL || state == NULL ||
        state->buffer == NULL)
        return;

    const uint32_t capacity  = state->buffer_len > 0u ? state->buffer_len : MAX_DELAY_SAMPLES;
    uint32_t       write_pos = apg_wrap_index_i64(state->write_pos, capacity);
    float delay = capacity > 1u ? apg_clamp_float(params->delay_samples, 0.0f, (float)capacity - 1.001f) : 0.0f;

    const uint32_t frames = apg_process_frames_or_default(info);
    for (uint32_t i = 0; i < frames; ++i) {
        float read_pos = (float)write_pos - delay;
        while (read_pos < 0.0f)
            read_pos += (float)capacity;
        while (read_pos >= (float)capacity)
            read_pos -= (float)capacity;

        const float    base  = floorf(read_pos);
        const uint32_t idx_a = (uint32_t)base;
        const uint32_t idx_b = idx_a + 1u == capacity ? 0u : idx_a + 1u;
        const float    frac  = read_pos - base;

        out->signal[i]           = state->buffer[idx_a] * (1.0f - frac) + state->buffer[idx_b] * frac;
        state->buffer[write_pos] = in->signal[i];
        write_pos                = write_pos + 1u == capacity ? 0u : write_pos + 1u;
    }

    state->write_pos = (int)write_pos;
}

void delay_fractional(
    delay_fractional_out_t    *out,
    delay_fractional_in_t     *in,
    delay_fractional_params_t *params,
    delay_fractional_state_t  *state
) {
    apg_process_info_t info = apg_process_info_default();
    delay_fractional_process(out, in, params, state, &info);
}
