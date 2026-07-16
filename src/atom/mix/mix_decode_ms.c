#include <atom/dsp_atoms.h>
#include <math.h>
#include <stddef.h>

void mix_decode_ms_process(
    mix_decode_ms_out_t          *out,
    const mix_decode_ms_in_t     *in,
    const mix_decode_ms_params_t *params,
    mix_decode_ms_state_t        *state,
    const apg_process_context_t  *info
) {
    if (!apg_process_context_valid(info))
        return;
    if (out == NULL || in == NULL || params == NULL || state == NULL)
        return;
    if (out->left == NULL || out->right == NULL || in->mid == NULL || in->side == NULL)
        return;

    const uint32_t frames    = apg_process_context_frames(info);
    float          inv_sqrt2 = (float)M_SQRT1_2;

    for (uint32_t i = 0; i < frames; ++i) {
        out->left[i]  = (in->mid[i] + in->side[i]) * inv_sqrt2;
        out->right[i] = (in->mid[i] - in->side[i]) * inv_sqrt2;
    }
}
