#include <atom/dsp_atoms.h>

#define CHUNK_LENGTH 512

void interpolation_cubic(interpolation_cubic_out_t out, interpolation_cubic_in_t in, void *params, void *state) {
    if (out.signal == NULL || in.signal_n1 == NULL || in.signal_a == NULL || 
        in.signal_b == NULL || in.signal_c == NULL || in.t == NULL) return;

    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        float t = in.t[i];
        float t2 = t * t;
        float t3 = t2 * t;

        float v0 = in.signal_n1[i];
        float v1 = in.signal_a[i];
        float v2 = in.signal_b[i];
        float v3 = in.signal_c[i];

        float a = -0.5f * v0 + 1.5f * v1 - 1.5f * v2 + 0.5f * v3;
        float b = v0 - 2.5f * v1 + 2.0f * v2 - 0.5f * v3;
        float c = -0.5f * v0 + 0.5f * v2;
        float d = v1;

        out.signal[i] = a * t3 + b * t2 + c * t + d;
    }
}
