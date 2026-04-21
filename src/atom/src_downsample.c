#include <atom/dsp_atoms.h>

#define CHUNK_LENGTH 512

void src_downsample(src_downsample_out_t out, src_downsample_in_t in, src_downsample_params_t params, void *state) {
    if (out.signal == NULL || in.signal == NULL) return;

    int M = params.factor;
    if (M < 1) M = 1;

    for (int i = 0; i < CHUNK_LENGTH / M; ++i) {
        out.signal[i] = in.signal[i * M];
    }
}
