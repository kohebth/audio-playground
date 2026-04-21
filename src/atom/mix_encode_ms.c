#include <atom/dsp_atoms.h>
#include <math.h>

#define CHUNK_LENGTH 512

void mix_encode_ms(mix_encode_ms_out_t out, mix_encode_ms_in_t in, void *params, void *state) {
    if (out.mid == NULL || out.side == NULL || in.left == NULL || in.right == NULL) return;

    float inv_sqrt2 = (float)M_SQRT1_2;

    for (int i = 0; i < CHUNK_LENGTH; i++) {
        out.mid[i] = (in.left[i] + in.right[i]) * inv_sqrt2;
        out.side[i] = (in.left[i] - in.right[i]) * inv_sqrt2;
    }
}
