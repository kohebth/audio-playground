#include <atom/dsp_atoms.h>
#include <math.h>

#define CHUNK_LENGTH 512

Signal amplitude_normalize(Signal signal, NormalizeParams params) {
    if (signal == NULL) return NULL;
    
    float peak = 0.0f;
    float sum_sq = 0.0f;
    
    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        float abs_val = fabsf(signal[i]);
        if (abs_val > peak) peak = abs_val;
        sum_sq += signal[i] * signal[i];
    }
    
    float current_level = 0.0f;
    if (params.mode == NORMALIZE_PEAK) {
        current_level = peak;
    } else {
        current_level = sqrtf(sum_sq / CHUNK_LENGTH);
    }
    
    if (current_level > 1e-6f) {
        float gain = params.target_level / current_level;
        for (int i = 0; i < CHUNK_LENGTH; ++i) {
            signal[i] *= gain;
        }
    }
    
    return signal;
}
