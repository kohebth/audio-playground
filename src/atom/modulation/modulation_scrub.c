#include <apgcore/dsp/dsp_safety.h>
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
    if (out->signal == NULL || in->buffer == NULL || in->position == NULL)
        return;

    const int      buffer_size = params->buffer_size;
    const uint32_t frames      = apg_process_frames_or_default(info);
    if (buffer_size < 2) {
        for (uint32_t i = 0; i < frames; ++i)
            out->signal[i] = 0.0f;
        return;
    }

    const float max_pos = (float)buffer_size - 1.001f;
    for (uint32_t i = 0; i < frames; ++i) {
        const float    pos    = apg_clamp_float(in->position[i], 0.0f, max_pos);
        const float    base   = floorf(pos);
        const uint32_t idx_a  = (uint32_t)base;
        const uint32_t idx_b  = idx_a + 1u < (uint32_t)buffer_size ? idx_a + 1u : idx_a;
        const float    frac   = pos - base;
        const float    a      = isfinite(in->buffer[idx_a]) ? in->buffer[idx_a] : 0.0f;
        const float    b      = isfinite(in->buffer[idx_b]) ? in->buffer[idx_b] : a;
        const float    sample = a * (1.0f - frac) + b * frac;
        out->signal[i]        = isfinite(sample) ? sample : 0.0f;
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
