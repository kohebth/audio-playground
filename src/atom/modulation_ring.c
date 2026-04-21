#include <atom/dsp_atoms.h>

#define CHUNK_LENGTH 512

void modulation_ring(modulation_ring_out_t out, modulation_ring_in_t in, void *params, void *state) {
    if (out.signal == NULL || in.signal == NULL || in.modulator == NULL) return;

    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        out.signal[i] = in.signal[i] * in.modulator[i];
    }
}
