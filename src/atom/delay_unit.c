#include <atom/dsp_atoms.h>

#define CHUNK_LENGTH 512

Signal delay_unit(Signal signal) {
    if (signal == NULL) return NULL;
    
    static float last_sample = 0.0f;
    static float buffer[CHUNK_LENGTH];
    
    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        float current = signal[i];
        buffer[i] = last_sample;
        last_sample = current;
    }
    
    return buffer;
}
