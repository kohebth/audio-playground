#include <apgcore/dsp/dsp_safety.h>
#include <atom/dsp_atoms.h>
#include <math.h>
#include <stdlib.h>

#include "atom_registry.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static inline float poly_blep(float phase, float phase_inc) {
    if (phase_inc <= 0.0f)
        return 0.0f;
    if (phase < phase_inc) {
        float t = phase / phase_inc;
        return t + t - t * t - 1.0f;
    }
    if (phase > 1.0f - phase_inc) {
        float t = (phase - 1.0f) / phase_inc;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

void generation_oscillator_process(
    generation_oscillator_out_t          *out,
    const generation_oscillator_in_t     *in,
    const generation_oscillator_params_t *params,
    generation_oscillator_state_t        *state,
    const apg_process_info_t             *info
) {
    if (out == NULL || in == NULL || params == NULL || state == NULL)
        return;
    if (out == NULL || out->signal == NULL || params == NULL || state == NULL)
        return;

    const uint32_t frames      = apg_process_frames_or_default(info);
    const float    sample_rate = apg_sample_rate_or_default(info);
    float          phase       = isfinite(state->phase) ? state->phase - floorf(state->phase) : 0.0f;

    for (uint32_t i = 0; i < frames; ++i) {
        float frequency       = in && in->frequency ? in->frequency[i] : params->frequency;
        frequency             = apg_clamp_float(frequency, 0.0f, sample_rate * 0.45f);
        const float phase_inc = frequency / sample_rate;

        float p = phase + (isfinite(params->phase_offset) ? params->phase_offset : 0.0f);
        p -= floorf(p);

        switch (params->waveform) {
        case WAVEFORM_SINE:
            out->signal[i] = sinf(2.0f * (float)M_PI * p);
            break;
        case WAVEFORM_SAW: {
            float sample = 2.0f * p - 1.0f;
            sample -= poly_blep(p, phase_inc);
            out->signal[i] = sample;
            break;
        }
        case WAVEFORM_SQUARE: {
            float sample = p < 0.5f ? 1.0f : -1.0f;
            sample += poly_blep(p, phase_inc);
            float second_edge = p + 0.5f;
            if (second_edge >= 1.0f)
                second_edge -= 1.0f;
            sample -= poly_blep(second_edge, phase_inc);
            out->signal[i] = sample;
            break;
        }
        case WAVEFORM_TRIANGLE:
            out->signal[i] = 4.0f * fabsf(p - floorf(p + 0.5f)) - 1.0f;
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
