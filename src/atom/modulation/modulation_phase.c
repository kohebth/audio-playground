#include <apgcore/dsp/dsp_safety.h>
#include <atom/dsp_atoms.h>
#include <math.h>
#include <stddef.h>

void modulation_phase_process(
    modulation_phase_out_t          *out,
    const modulation_phase_in_t     *in,
    const modulation_phase_params_t *params,
    modulation_phase_state_t        *state,
    const apg_process_info_t        *info
) {
    if (out == NULL || in == NULL || params == NULL || state == NULL)
        return;
    if (out == NULL || in == NULL || params == NULL || state == NULL || out->signal == NULL || in->signal == NULL ||
        in->modulator == NULL || state->buffer == NULL)
        return;

    const uint32_t capacity = state->buffer_len > 0u ? state->buffer_len : APG_MODULATION_DELAY_CAPACITY;
    if (capacity < 4u)
        return;
    const uint32_t frames    = apg_process_frames_or_default(info);
    uint32_t       write_pos = apg_wrap_index_i64(state->write_pos, capacity);
    const float    max_delay = (float)capacity - 2.0f;
    const float    depth     = isfinite(params->depth) ? apg_clamp_float(params->depth, 0.0f, max_delay) : 0.0f;

    for (uint32_t i = 0; i < frames; ++i) {
        const float modulator     = isfinite(in->modulator[i]) ? apg_clamp_float(in->modulator[i], -1.0f, 1.0f) : 0.0f;
        const float scaled_delay  = depth * (1.0f + modulator * 0.5f);
        const float delay_samples = apg_clamp_float(isfinite(scaled_delay) ? scaled_delay : 0.0f, 0.0f, max_delay);

        float read_pos = (float)write_pos - delay_samples;
        if (read_pos < 0.0f)
            read_pos += (float)capacity;

        const float    read_floor = floorf(read_pos);
        const uint32_t idx1       = (uint32_t)read_floor;
        const uint32_t idx0       = idx1 > 0u ? idx1 - 1u : capacity - 1u;
        const uint32_t idx2       = idx1 + 1u == capacity ? 0u : idx1 + 1u;
        const uint32_t idx3       = idx2 + 1u == capacity ? 0u : idx2 + 1u;

        float frac  = read_pos - read_floor;
        float frac2 = frac * frac;
        float frac3 = frac2 * frac;

        float v0 = state->buffer[idx0];
        float v1 = state->buffer[idx1];
        float v2 = state->buffer[idx2];
        float v3 = state->buffer[idx3];
        if (!isfinite(v0)) {
            v0                  = 0.0f;
            state->buffer[idx0] = 0.0f;
        }
        if (!isfinite(v1)) {
            v1                  = 0.0f;
            state->buffer[idx1] = 0.0f;
        }
        if (!isfinite(v2)) {
            v2                  = 0.0f;
            state->buffer[idx2] = 0.0f;
        }
        if (!isfinite(v3)) {
            v3                  = 0.0f;
            state->buffer[idx3] = 0.0f;
        }

        float a = -0.5f * v0 + 1.5f * v1 - 1.5f * v2 + 0.5f * v3;
        float b = v0 - 2.5f * v1 + 2.0f * v2 - 0.5f * v3;
        float c = -0.5f * v0 + 0.5f * v2;
        float d = v1;

        float output = a * frac3 + b * frac2 + c * frac + d;
        if (!isfinite(output))
            output = 0.0f;
        const float input = isfinite(in->signal[i]) ? in->signal[i] : 0.0f;

        out->signal[i]           = output;
        state->buffer[write_pos] = input;
        write_pos                = write_pos + 1u == capacity ? 0u : write_pos + 1u;
    }

    state->write_pos = (int)write_pos;
}
