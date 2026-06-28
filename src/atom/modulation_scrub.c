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
    if (out->signal == NULL || in->buffer == NULL || in->position == NULL)
        return;

    const uint32_t frames = apg_process_frames_or_default(info);

    for (uint32_t i = 0; i < frames; ++i) {
        float pos = in->position[i];
        if (pos < 0.0f)
            pos = 0.0f;
        if (pos > (float)params->buffer_size - 2.0f)
            pos = (float)params->buffer_size - 2.0f;

        uint32_t idx_a = (uint32_t)floorf(pos);
        uint32_t idx_b = idx_a + 1;
        float    frac  = pos - floorf(pos);

        out->signal[i] = in->buffer[idx_a] * (1.0f - frac) + in->buffer[idx_b] * frac;
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
