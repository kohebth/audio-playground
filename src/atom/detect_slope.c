#include <atom/dsp_atoms.h>

#define CHUNK_LENGTH 512

Signal detect_slope(Signal signal) {
    if (signal == NULL) return NULL;
    
    static float last_sample = 0.0f;
    static float out_buffer[CHUNK_LENGTH];
    
    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        out_buffer[i] = signal[i] - last_sample;
        last_sample = signal[i];
    }
    
    return out_buffer;
}
