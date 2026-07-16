#include <apgcore/dsp/dsp_safety.h>
#include <atom/dsp_atoms.h>
#include <math.h>
#include <stddef.h>

void modulation_frequency_process(
    modulation_frequency_out_t          *out,
    const modulation_frequency_in_t     *in,
    const modulation_frequency_params_t *params,
    modulation_frequency_state_t        *state,
    const apg_process_context_t         *info
) {
    if (!apg_process_context_valid(info))
        return;
    if (out == NULL || in == NULL || params == NULL || state == NULL)
        return;
    if (out == NULL || in == NULL || params == NULL || state == NULL || out->signal == NULL || in->signal == NULL ||
        in->modulator == NULL || state->buffer == NULL)
        return;

    const uint32_t capacity = state->buffer_len > 0u ? state->buffer_len : APG_MODULATION_DELAY_CAPACITY;
    if (capacity < 2u)
        return;
    const uint32_t frames        = apg_process_context_frames(info);
    uint32_t       write_pos     = apg_wrap_index_i64(state->write_pos, capacity);
    float          current_delay = apg_clamp_float(state->current_delay, 0.0f, (float)capacity - 2.0f);
    const float    max_depth     = (float)capacity - 2.0f;
    const float    depth = isfinite(params->depth) ? apg_clamp_float(params->depth, -max_depth, max_depth) : 0.0f;

    for (uint32_t i = 0; i < frames; ++i) {
        const float modulator = isfinite(in->modulator[i]) ? apg_clamp_float(in->modulator[i], -1.0f, 1.0f) : 0.0f;
        const float delta     = modulator * depth;
        current_delay         = apg_clamp_float(current_delay + delta, 0.0f, max_depth);

        float read_pos = (float)write_pos - current_delay;
        if (read_pos < 0.0f)
            read_pos += (float)capacity;

        const float    read_floor = floorf(read_pos);
        const uint32_t idx_a      = (uint32_t)read_floor;
        const uint32_t idx_b      = idx_a + 1u == capacity ? 0u : idx_a + 1u;
        const float    frac       = read_pos - read_floor;
        float          sample_a   = state->buffer[idx_a];
        float          sample_b   = state->buffer[idx_b];
        if (!isfinite(sample_a)) {
            sample_a             = 0.0f;
            state->buffer[idx_a] = 0.0f;
        }
        if (!isfinite(sample_b)) {
            sample_b             = 0.0f;
            state->buffer[idx_b] = 0.0f;
        }

        float output = sample_a * (1.0f - frac) + sample_b * frac;
        if (!isfinite(output))
            output = 0.0f;
        const float input = isfinite(in->signal[i]) ? in->signal[i] : 0.0f;

        out->signal[i]           = output;
        state->buffer[write_pos] = input;
        write_pos                = write_pos + 1u == capacity ? 0u : write_pos + 1u;
    }

    state->write_pos     = (int)write_pos;
    state->current_delay = current_delay;
}
