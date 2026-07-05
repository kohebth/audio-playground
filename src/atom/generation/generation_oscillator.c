#include <atom/dsp_atoms.h>
#include <math.h>
#include <stdlib.h>

#include "atom_registry.h"

void generation_oscillator_process(
    generation_oscillator_out_t    *out,
    generation_oscillator_in_t     *in,
    generation_oscillator_params_t *params,
    generation_oscillator_state_t  *state,
    const apg_process_info_t       *info
) {
    if (out->signal == NULL || state == NULL)
        return;

    const uint32_t frames = apg_process_frames_or_default(info);
    float          phase  = state->phase;

    for (uint32_t i = 0; i < frames; ++i) {
        float freq = (in->frequency != NULL) ? in->frequency[i] : params->frequency;
        float sr   = params->sample_rate;
        if (sr <= 0.0f)
            sr = 48000.0f;
        float phase_inc = freq / sr;
        if (!isfinite(phase_inc))
            phase_inc = 0.0f;

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

void generation_oscillator(
    generation_oscillator_out_t    *out,
    generation_oscillator_in_t     *in,
    generation_oscillator_params_t *params,
    generation_oscillator_state_t  *state
) {
    const apg_process_info_t info = apg_process_info_default();
    generation_oscillator_process(out, in, params, state, &info);
}
