#include <atom/dsp_atoms.h>

#define CHUNK_LENGTH 512

void mix_pan_stereo(mix_pan_stereo_out_t out, mix_pan_stereo_in_t in, mix_pan_stereo_params_t params, void *state) {
    if (out.left == NULL || out.right == NULL || in.signal == NULL) return;

    float p = params.position; // 0.0 for left, 1.0 for right, 0.5 central
    if (p < 0.0f) p = 0.0f;
    if (p > 1.0f) p = 1.0f;

    float g_l = 1.0f - p;
    float g_r = p;

    for (int i = 0; i < CHUNK_LENGTH; i++) {
        out.left[i] = in.signal[i] * g_l;
        out.right[i] = in.signal[i] * g_r;
    }
}
