#include <atom/dsp_atoms.h>
#include <stddef.h>

#define CHUNK_LENGTH 512

void generation_dc(
    generation_dc_out_t *out, generation_dc_in_t *in, generation_dc_params_t *params, generation_dc_state_t *state
) {
    if (out->signal == NULL)
        return;
    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        out->signal[i] = params->value;
    }
}
