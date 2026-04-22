#include <atom/dsp_atoms.h>
#include <stdlib.h>

#define CHUNK_LENGTH 512

void generation_noise(
    generation_noise_out_t    *out,
    generation_noise_in_t     *in,
    generation_noise_params_t *params,
    generation_noise_state_t  *state
) {
    if (out->signal == NULL || state == NULL)
        return;

    uint32_t seed       = state->seed;
    float    prev_value = state->prev_value;

    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        // Linear Congruential Generator
        seed        = seed * 1664525u + 1013904223u;
        float white = ((float)seed / 4294967296.0f) * 2.0f - 1.0f;

        float value = 0.0f;
        switch (params->color) {
        case WAVEFORM_NOISE_WHITE:
            value = white;
            break;
        case WAVEFORM_NOISE_PINK:
            // Crude pink noise approximation
            value = (prev_value + white) * 0.5f;
            break;
        case WAVEFORM_NOISE_BROWN:
            // Brownian noise
            value = prev_value + white * 0.1f;
            if (value > 1.0f)
                value = 1.0f;
            if (value < -1.0f)
                value = -1.0f;
            break;
        default:
            value = white;
            break;
        }

        out->signal[i] = value * params->amplitude;
        prev_value     = value;
    }

    state->seed       = seed;
    state->prev_value = prev_value;
}
