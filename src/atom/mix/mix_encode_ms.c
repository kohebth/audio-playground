#include <atom/dsp_atoms.h>
#include <math.h>
#include <stddef.h>

void mix_encode_ms_process(
    mix_encode_ms_out_t          *out,
    const mix_encode_ms_in_t     *in,
    const mix_encode_ms_params_t *params,
    mix_encode_ms_state_t        *state,
    const apg_process_context_t  *info
) {
    if (!apg_process_context_valid(info))
        return;
    if (out == NULL || in == NULL || params == NULL || state == NULL)
        return;
    if (out->mid == NULL || out->side == NULL || in->left == NULL || in->right == NULL)
        return;

    const uint32_t frames    = apg_process_context_frames(info);
    float          inv_sqrt2 = (float)M_SQRT1_2;

    for (uint32_t i = 0; i < frames; ++i) {
        out->mid[i]  = (in->left[i] + in->right[i]) * inv_sqrt2;
        out->side[i] = (in->left[i] - in->right[i]) * inv_sqrt2;
    }
}
