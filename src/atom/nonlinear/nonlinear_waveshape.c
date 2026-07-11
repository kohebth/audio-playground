#include <atom/dsp_atoms.h>
#include <math.h>
#include <stddef.h>

#define TABLE_SIZE 1024

void nonlinear_waveshape_process(
    nonlinear_waveshape_out_t    *out,
    nonlinear_waveshape_in_t     *in,
    nonlinear_waveshape_params_t *params,
    nonlinear_waveshape_state_t  *state,
    const apg_process_info_t     *info
) {
    if (out == NULL || in == NULL || params == NULL || state == NULL)
        return;
    if (out->signal == NULL || in->signal == NULL || params->transfer_table == NULL)
        return;

    const uint32_t frames = apg_process_frames_or_default(info);
    int            size   = params->table_size;
    if (size < 2)
        return;

    for (uint32_t i = 0; i < frames; ++i) {
        float x   = in->signal[i];
        float pos = (x + 1.0f) * 0.5f * (float)(size - 1);

        if (pos < 0.0f)
            pos = 0.0f;
        if (pos > (float)size - 2.0f)
            pos = (float)size - 2.0f;

        uint32_t idx_a = (uint32_t)floorf(pos);
        uint32_t idx_b = idx_a + 1;
        float    frac  = pos - floorf(pos);

        out->signal[i] = params->transfer_table[idx_a] * (1.0f - frac) + params->transfer_table[idx_b] * frac;
    }
}

void nonlinear_waveshape(
    nonlinear_waveshape_out_t    *out,
    nonlinear_waveshape_in_t     *in,
    nonlinear_waveshape_params_t *params,
    nonlinear_waveshape_state_t  *state
) {
    const apg_process_info_t info = apg_process_info_default();
    nonlinear_waveshape_process(out, in, params, state, &info);
}
