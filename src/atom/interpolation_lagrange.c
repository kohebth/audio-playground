#include <atom/dsp_atoms.h>

#define CHUNK_LENGTH 512

void interpolation_lagrange(interpolation_lagrange_out_t out, interpolation_lagrange_in_t in, interpolation_lagrange_params_t params, void *state) {
    if (out.signal == NULL || in.samples == NULL || in.t == NULL) return;

    int n = params.order;
    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        float pos = in.t[i];
        int base = (int)floorf(pos) - n / 2;
        float frac = pos - floorf(pos) + (float)(n / 2);
        
        float result = 0.0f;
        for (int k = 0; k <= n; ++k) {
            float l_k = 1.0f;
            for (int j = 0; j <= n; ++j) {
                if (k == j) continue;
                l_k *= (frac - (float)j) / ((float)k - (float)j);
            }
            result += in.samples[base + k] * l_k;
        }
        out.signal[i] = result;
    }
}
