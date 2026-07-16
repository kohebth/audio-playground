#include <atom/dsp_atoms.h>
#include <stddef.h>

void generation_dc_process(
    generation_dc_out_t          *out,
    const generation_dc_in_t     *in,
    const generation_dc_params_t *params,
    generation_dc_state_t        *state,
    const apg_process_context_t  *info
) {
    if (!apg_process_context_valid(info))
        return;
    if (out == NULL || in == NULL || params == NULL || state == NULL)
        return;
    (void)in;
    (void)state;
    if (out->signal == NULL || params == NULL)
        return;

    const uint32_t frames = apg_process_context_frames(info);
    for (uint32_t i = 0; i < frames; ++i) {
        out->signal[i] = params->value;
    }
}
