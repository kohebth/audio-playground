#include <atom/dsp_atoms.h>
#include <stdlib.h>
#include <string.h>

#define CHUNK_LENGTH 512
#define MAX_UPSAMPLE_FACTOR 8

void src_upsample(src_upsample_out_t out, src_upsample_in_t in, src_upsample_params_t params, void *state) {
    if (out.signal == NULL || in.signal == NULL) return;

    int L = params.factor;
    if (L < 1) L = 1;

    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        out.signal[i * L] = in.signal[i];
        for (int j = 1; j < L; ++j) {
            out.signal[i * L + j] = 0.0f;
        }
    }
}
