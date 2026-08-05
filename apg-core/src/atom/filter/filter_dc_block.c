#include <atom/dsp_atoms.h>
#include <stddef.h>

void filter_dc_block_process(
    filter_dc_block_out_t          *out,
    const filter_dc_block_in_t     *in,
    const filter_dc_block_params_t *params,
    filter_dc_block_state_t        *state,
    const apg_process_context_t    *info
) {
    if (!apg_process_context_valid(info))
        return;
    if (out == NULL || in == NULL || params == NULL || state == NULL)
        return;
    if (out->signal == NULL || in->signal == NULL || state == NULL)
        return;

    const uint32_t frames = apg_process_context_frames(info);
    float          x1     = state->prev_input;
    float          y1     = state->prev_output;
    float          R      = params->coefficient;

    for (uint32_t i = 0; i < frames; ++i) {
        float x0       = in->signal[i];
        float y0       = x0 - x1 + R * y1;
        out->signal[i] = y0;
        x1             = x0;
        y1             = y0;
    }

    state->prev_input  = x1;
    state->prev_output = y1;
}
