#include <atom/dsp_atoms.h>

#define CHUNK_LENGTH 512

Signal filter_differentiate(Signal signal) {
    if (signal == NULL) return NULL;
    
    static float last_x = 0.0f;
    static float out_buffer[CHUNK_LENGTH];
    
    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        float x0 = signal[i];
        out_buffer[i] = x0 - last_x;
        last_x = x0;
    }
    
    return out_buffer;
}
