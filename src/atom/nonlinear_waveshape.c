#include <atom/dsp_atoms.h>
#include <math.h>
#include <stddef.h>

#define CHUNK_LENGTH 512
#define TABLE_SIZE   1024

void nonlinear_waveshape(
    nonlinear_waveshape_out_t    *out,
    nonlinear_waveshape_in_t     *in,
    nonlinear_waveshape_params_t *params,
    nonlinear_waveshape_state_t  *state
) {
    if (out->signal == NULL || in->signal == NULL || params->transfer_table == NULL)
        return;

    int size = params->table_size;
    for (int i = 0; i < CHUNK_LENGTH; i++) {
        // Map [-1.0, 1.0] to [0, table_size - 1]
        float x   = in->signal[i];
        float pos = (x + 1.0f) * 0.5f * (float)(size - 1);

        if (pos < 0.0f)
            pos = 0.0f;
        if (pos > (float)size - 2.0f)
            pos = (float)size - 2.0f;

        uint32_t idx_a = (uint32_t)floorf(pos);
        uint32_t idx_b = idx_a + 1;
        float    frac  = pos - floorf(pos);

        // Linear interpolation in the transfer table
        out->signal[i] = params->transfer_table[idx_a] * (1.0f - frac) + params->transfer_table[idx_b] * frac;
    }
}
