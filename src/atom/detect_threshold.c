#include <atom/dsp_atoms.h>
#include <stddef.h>

#define CHUNK_LENGTH 512

void detect_threshold(
    detect_threshold_out_t    *out,
    detect_threshold_in_t     *in,
    detect_threshold_params_t *params,
    detect_threshold_state_t  *state
) {
    if (out->gate == NULL || in->signal == NULL)
        return;

    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        out->gate[i] = (in->signal[i] >= params->threshold) ? 1.0f : 0.0f;
    }
}
