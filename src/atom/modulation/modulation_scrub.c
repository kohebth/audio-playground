#include <atom/dsp_atoms.h>
#include <math.h>
#include <stddef.h>

void modulation_scrub_process(
    modulation_scrub_out_t    *out,
    modulation_scrub_in_t     *in,
    modulation_scrub_params_t *params,
    modulation_scrub_state_t  *state,
    const apg_process_info_t  *info
) {
    if (out == NULL || in == NULL || params == NULL || state == NULL)
        return;
    if (out == NULL || in == NULL || params == NULL || out->signal == NULL || in->buffer == NULL ||
        in->position == NULL)
        return;

    const uint32_t frames = apg_process_frames_or_default(info);
    if (params->buffer_size < 2) {
        for (uint32_t i = 0; i < frames; ++i)
            out->signal[i] = 0.0f;
        return;
    }
    const float max_position = (float)params->buffer_size - 2.0f;

    for (uint32_t i = 0; i < frames; ++i) {
        float pos = isfinite(in->position[i]) ? in->position[i] : 0.0f;
        if (pos < 0.0f)
            pos = 0.0f;
        if (pos > max_position)
            pos = max_position;

        uint32_t idx_a = (uint32_t)floorf(pos);
        uint32_t idx_b = idx_a + 1;
        float    frac  = pos - floorf(pos);

        const float sample_a = isfinite(in->buffer[idx_a]) ? in->buffer[idx_a] : 0.0f;
        const float sample_b = isfinite(in->buffer[idx_b]) ? in->buffer[idx_b] : 0.0f;
        const float sample   = sample_a * (1.0f - frac) + sample_b * frac;
        out->signal[i]       = isfinite(sample) ? sample : 0.0f;
    }
}

void modulation_scrub(
    modulation_scrub_out_t    *out,
    modulation_scrub_in_t     *in,
    modulation_scrub_params_t *params,
    modulation_scrub_state_t  *state
) {
    const apg_process_info_t info = apg_process_info_default();
    modulation_scrub_process(out, in, params, state, &info);
}
