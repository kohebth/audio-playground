#ifndef AUDIO_PLAYGROUND_PRIMITIVE_KERNELS_H
#define AUDIO_PLAYGROUND_PRIMITIVE_KERNELS_H

#include <apgcore/runtime/process.h>

#include <stdint.h>

void apg_oscillator_kernel(
    float                       *output,
    const float                 *frequency_signal,
    float                        frequency,
    int                          waveform,
    float                        phase_offset,
    float                       *phase,
    const apg_process_context_t *context
);

void apg_difference_kernel(float *output, const float *input, float *previous, uint32_t frames);
void apg_integrate_kernel(float *output, const float *input, float leakage, float *accumulator, uint32_t frames);
void apg_crossfade_kernel(
    float *output, const float *signal_a, const float *signal_b, float position, int curve, uint32_t frames
);

#endif // AUDIO_PLAYGROUND_PRIMITIVE_KERNELS_H
