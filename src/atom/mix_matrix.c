#include <atom/dsp_atoms.h>
#include <string.h>

#define CHUNK_LENGTH 512
#define MAX_CHANNELS 8

void mix_matrix(mix_matrix_out_t out, mix_matrix_in_t in, mix_matrix_params_t params, void *state) {
    if (out.signals == NULL || in.signals == NULL || params.coefficients == NULL) return;

    for (int j = 0; j < params.num_out; j++) {
        if (out.signals[j] == NULL) continue;
        memset(out.signals[j], 0, CHUNK_LENGTH * sizeof(float));
        for (int i = 0; i < params.num_in; i++) {
            if (in.signals[i] == NULL) continue;
            float g = params.coefficients[j][i];
            for (int k = 0; k < CHUNK_LENGTH; k++) {
                out.signals[j][k] += in.signals[i][k] * g;
            }
        }
    }
}
