#include <atom/dsp_atoms.h>
#include <math.h>
#include <stddef.h>

void nonlinear_bitcrush_process(
    nonlinear_bitcrush_out_t          *out,
    const nonlinear_bitcrush_in_t     *in,
    const nonlinear_bitcrush_params_t *params,
    nonlinear_bitcrush_state_t        *state,
    const apg_process_context_t       *info
) {
    if (!apg_process_context_valid(info))
        return;
    if (out == NULL || in == NULL || params == NULL || state == NULL)
        return;
    if (out->signal == NULL || in->signal == NULL)
        return;

    const uint32_t frames = apg_process_context_frames(info);
    float          levels = powf(2.0f, params->bit_depth);

    for (uint32_t i = 0; i < frames; ++i) {
        out->signal[i] = roundf(in->signal[i] * levels) / levels;
    }
}
