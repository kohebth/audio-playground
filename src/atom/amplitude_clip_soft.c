#include <atom/dsp_atoms.h>
#include <fast_math.h>
#include <math.h>

#define CHUNK_LENGTH 512

Signal amplitude_clip_soft(Signal signal, ClipParams params, float curve) {
    if (signal == NULL) return NULL;
    
    // Simple soft clipping using tanh-like function if curve > 0
    // Otherwise fall back to a simple saturator
    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        float x = signal[i] / params.threshold;
        if (curve > 0) {
            signal[i] = params.threshold * tanhf(x * curve) / tanhf(curve);
        } else {
            // Default soft knee
            if (x > 1.0f) signal[i] = params.threshold;
            else if (x < -1.0f) signal[i] = -params.threshold;
            else signal[i] = params.threshold * (x - (x * x * x) / 3.0f);
        }
    }
    
    return signal;
}
