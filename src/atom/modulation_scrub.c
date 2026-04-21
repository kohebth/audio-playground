#include <atom/dsp_atoms.h>
#include <math.h>

#define CHUNK_LENGTH 512

void modulation_scrub(modulation_scrub_out_t out, modulation_scrub_in_t in, modulation_scrub_params_t params, void *state) {
    if (out.signal == NULL || in.buffer == NULL || in.position == NULL) return;

    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        float pos = in.position[i];
        if (pos < 0.0f) pos = 0.0f;
        if (pos > (float)params.buffer_size - 2.0f) pos = (float)params.buffer_size - 2.0f;

        uint32_t idx_a = (uint32_t)floorf(pos);
        uint32_t idx_b = idx_a + 1;
        float frac = pos - floorf(pos);

        out.signal[i] = in.buffer[idx_a] * (1.0f - frac) + in.buffer[idx_b] * frac;
    }
}
