#include <atom/dsp_atoms.h>

#define CHUNK_LENGTH 512

void amplitude_subtract(amplitude_subtract_out_t out, amplitude_subtract_in_t in, void *params, void *state) {
    if (out.signal == NULL || in.signal_a == NULL || in.signal_b == NULL) return;
    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        out.signal[i] = in.signal_a[i] - in.signal_b[i];
    }
}
