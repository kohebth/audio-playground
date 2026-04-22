#include <atom/dsp_atoms.h>
#include <math.h>
#include <stddef.h>

#define CHUNK_LENGTH 512

void mix_decode_ms(
    mix_decode_ms_out_t *out, mix_decode_ms_in_t *in, mix_decode_ms_params_t *params, mix_decode_ms_state_t *state
) {
    if (out->left == NULL || out->right == NULL || in->mid == NULL || in->side == NULL)
        return;

    float inv_sqrt2 = (float)M_SQRT1_2;

    for (int i = 0; i < CHUNK_LENGTH; i++) {
        out->left[i]  = (in->mid[i] + in->side[i]) * inv_sqrt2;
        out->right[i] = (in->mid[i] - in->side[i]) * inv_sqrt2;
    }
}
