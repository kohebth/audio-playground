#include <apgcore/dsp/dsp_safety.h>
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
    if (size < 2) {
        for (uint32_t i = 0; i < frames; ++i)
            out->signal[i] = isfinite(in->signal[i]) ? in->signal[i] : 0.0f;
        return;
    }
    if (size > TABLE_SIZE)
        size = TABLE_SIZE;

    for (uint32_t i = 0; i < frames; ++i) {
        const float    x     = apg_clamp_float(in->signal[i], -1.0f, 1.0f);
        const float    pos   = apg_clamp_float((x + 1.0f) * 0.5f * (float)(size - 1), 0.0f, (float)(size - 2));
        uint32_t       idx_a = (uint32_t)floorf(pos);
        const uint32_t idx_b = idx_a + 1u;
        const float    frac  = pos - (float)idx_a;
        const float    a     = isfinite(params->transfer_table[idx_a]) ? params->transfer_table[idx_a] : 0.0f;
        const float    b     = isfinite(params->transfer_table[idx_b]) ? params->transfer_table[idx_b] : a;
        out->signal[i]       = a * (1.0f - frac) + b * frac;
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
