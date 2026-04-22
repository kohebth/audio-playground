#include <atom/dsp_atoms.h>
#include <stddef.h>

#define CHUNK_LENGTH 512

void modulation_amplitude(
    modulation_amplitude_out_t    *out,
    modulation_amplitude_in_t     *in,
    modulation_amplitude_params_t *params,
    modulation_amplitude_state_t  *state
) {
    if (out->signal == NULL || in->signal == NULL || in->modulator == NULL)
        return;

    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        out->signal[i] = in->signal[i] * (1.0f + params->depth * in->modulator[i]);
    }
}
