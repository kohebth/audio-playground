#include <atom/dsp_atoms.h>

#define CHUNK_LENGTH 512

void interpolation_linear(interpolation_linear_out_t out, interpolation_linear_in_t in, void *params, void *state) {
    if (out.signal == NULL || in.signal_a == NULL || in.signal_b == NULL || in.t == NULL) return;

    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        out.signal[i] = in.signal_a[i] * (1.0f - in.t[i]) + in.signal_b[i] * in.t[i];
    }
}
