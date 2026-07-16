#include <atom/dsp_atoms.h>
#include <stdlib.h>

void generation_noise_process(
    generation_noise_out_t          *out,
    const generation_noise_in_t     *in,
    const generation_noise_params_t *params,
    generation_noise_state_t        *state,
    const apg_process_info_t        *info
) {
    if (out == NULL || in == NULL || params == NULL || state == NULL)
        return;
    if (out->signal == NULL || state == NULL)
        return;

    const uint32_t frames     = apg_process_frames_or_default(info);
    uint32_t       seed       = state->seed;
    float          prev_value = state->prev_value;

    for (uint32_t i = 0; i < frames; ++i) {
        seed        = seed * 1664525u + 1013904223u;
        float white = ((float)seed / 4294967296.0f) * 2.0f - 1.0f;

        float value = 0.0f;
        switch (params->color) {
        case WAVEFORM_NOISE_WHITE:
            value = white;
            break;
        case WAVEFORM_NOISE_PINK:
            value = (prev_value + white) * 0.5f;
            break;
        case WAVEFORM_NOISE_BROWN:
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
