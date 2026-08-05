#include "primitive_kernels.h"

#include <apgcore/dsp/dsp_safety.h>
#include <atom/types/dsp_enums.h>

#include <math.h>
#include <stddef.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static float poly_blep(float phase, float phase_increment) {
    if (phase_increment <= 0.0f)
        return 0.0f;
    if (phase < phase_increment) {
        const float t = phase / phase_increment;
        return t + t - t * t - 1.0f;
    }
    if (phase > 1.0f - phase_increment) {
        const float t = (phase - 1.0f) / phase_increment;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

void apg_oscillator_kernel(
    float                       *output,
    const float                 *frequency_signal,
    float                        frequency,
    int                          waveform,
    float                        phase_offset,
    float                       *phase,
    const apg_process_context_t *context
) {
    if (!output || !phase || !apg_process_context_valid(context))
        return;

    const uint32_t frames      = apg_process_context_frames(context);
    const float    sample_rate = apg_process_context_sample_rate(context);
    float          current     = isfinite(*phase) ? *phase - floorf(*phase) : 0.0f;
    const float    offset      = isfinite(phase_offset) ? phase_offset : 0.0f;

    for (uint32_t i = 0u; i < frames; i++) {
        float current_frequency = frequency_signal ? frequency_signal[i] : frequency;
        current_frequency       = apg_clamp_float(current_frequency, 0.0f, sample_rate * 0.45f);
        const float increment   = current_frequency / sample_rate;
        float       p           = current + offset;
        p -= floorf(p);

        switch (waveform) {
        case WAVEFORM_SINE:
            output[i] = sinf(2.0f * (float)M_PI * p);
            break;
        case WAVEFORM_SAW:
            output[i] = 2.0f * p - 1.0f - poly_blep(p, increment);
            break;
        case WAVEFORM_SQUARE: {
            float sample = p < 0.5f ? 1.0f : -1.0f;
            sample += poly_blep(p, increment);
            float second_edge = p + 0.5f;
            if (second_edge >= 1.0f)
                second_edge -= 1.0f;
            output[i] = sample - poly_blep(second_edge, increment);
            break;
        }
        case WAVEFORM_TRIANGLE:
            output[i] = 4.0f * fabsf(p - floorf(p + 0.5f)) - 1.0f;
            break;
        default:
            output[i] = 0.0f;
            break;
        }

        current += increment;
        current -= floorf(current);
    }

    *phase = current;
}

void apg_difference_kernel(float *output, const float *input, float *previous, uint32_t frames) {
    if (!output || !input || !previous)
        return;
    float last = *previous;
    for (uint32_t i = 0u; i < frames; i++) {
        const float sample = input[i];
        output[i]          = sample - last;
        last               = sample;
    }
    *previous = last;
}

void apg_integrate_kernel(float *output, const float *input, float leakage, float *accumulator, uint32_t frames) {
    if (!output || !input || !accumulator)
        return;
    leakage   = isfinite(leakage) ? apg_clamp_float(leakage, 0.0f, 1.0f) : 1.0f;
    float sum = *accumulator;
    for (uint32_t i = 0u; i < frames; i++) {
        sum       = input[i] + leakage * sum;
        output[i] = sum;
    }
    *accumulator = sum;
}

void apg_crossfade_kernel(
    float *output, const float *signal_a, const float *signal_b, float position, int curve, uint32_t frames
) {
    if (!output || !signal_a || !signal_b)
        return;
    const float t = isfinite(position) ? apg_clamp_float(position, 0.0f, 1.0f) : 0.0f;
    float       gain_a;
    float       gain_b;
    if (curve == 1) {
        gain_a = cosf(t * (float)M_PI * 0.5f);
        gain_b = sinf(t * (float)M_PI * 0.5f);
    } else {
        gain_a = 1.0f - t;
        gain_b = t;
    }
    for (uint32_t i = 0u; i < frames; i++)
        output[i] = gain_a * signal_a[i] + gain_b * signal_b[i];
}
