#include <atom/dsp_atoms.h>

#define CHUNK_LENGTH 512

Signal detect_threshold(Signal signal, ThresholdParams params) {
    if (signal == NULL) return NULL;
    
    static float out_buffer[CHUNK_LENGTH];
    
    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        out_buffer[i] = (signal[i] >= params.threshold) ? 1.0f : 0.0f;
    }
    
    return out_buffer;
}
