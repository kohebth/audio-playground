#include <atom/dsp_atoms.h>

#define CHUNK_LENGTH 512

void generation_dc(generation_dc_out_t out, void *in, generation_dc_params_t params, void *state) {
    if (out.signal == NULL) return;
    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        out.signal[i] = params.value;
    }
}
