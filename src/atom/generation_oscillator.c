#include <atom/dsp_atoms.h>
#include <math.h>
#include <stdlib.h>

#define CHUNK_LENGTH 512

Signal generation_oscillator(OscillatorParams params) {
    static float buffer[CHUNK_LENGTH];
    float phase = params.phase;
    float phase_inc = params.frequency / params.sample_rate;
    
    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        switch (params.waveform) {
            case WAVEFORM_SINE:
                buffer[i] = sinf(2.0f * M_PI * phase);
                break;
            case WAVEFORM_SAW:
                buffer[i] = 2.0f * (phase - floorf(phase + 0.5f));
                break;
            case WAVEFORM_SQUARE:
                buffer[i] = (phase - floorf(phase) < 0.5f) ? 1.0f : -1.0f;
                break;
            case WAVEFORM_TRIANGLE:
                buffer[i] = 4.0f * fabsf(phase - floorf(phase + 0.75f) + 0.25f) - 1.0f;
                break;
            default:
                buffer[i] = 0.0f;
                break;
        }
        
        phase += phase_inc;
        if (phase >= 1.0f) phase -= 1.0f;
    }
    
    // Note: The updated phase is not returned to the caller as per the current signature.
    // This atom expects the caller to calculate the next starting phase.
    
    return buffer;
}
