#include <atom/dsp_atoms.h>
#include <math.h>
#include <stddef.h>

#define CHUNK_LENGTH 512

void generation_lfo(
    generation_lfo_out_t *out, generation_lfo_in_t *in, generation_lfo_params_t *params, generation_lfo_state_t *state
) {
    if (out->signal == NULL || state == NULL)
        return;

    float phase     = state->phase;
    float phase_inc = params->frequency / params->sample_rate;

    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        float p = phase + params->phase_offset;
        p -= floorf(p);

        switch (params->waveform) {
        case WAVEFORM_SINE:
            out->signal[i] = sinf(2.0f * (float)M_PI * p);
            break;
        case WAVEFORM_SAW:
            out->signal[i] = 2.0f * (p - floorf(p + 0.5f));
            break;
        case WAVEFORM_SQUARE:
            out->signal[i] = (p < 0.5f) ? 1.0f : -1.0f;
            break;
        case WAVEFORM_TRIANGLE:
            out->signal[i] = 4.0f * fabsf(p - floorf(p + 0.75f) + 0.25f) - 1.0f;
            break;
        default:
            out->signal[i] = 0.0f;
            break;
        }

        phase += phase_inc;
        phase -= floorf(phase);
    }

    state->phase = phase;
}
