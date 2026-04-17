#include <atom/dsp_atoms.h>

#define CHUNK_LENGTH 512

Signal detect_zero_crossing(Signal signal) {
    if (signal == NULL) return NULL;
    
    static float last_sample = 0.0f;
    static float out_buffer[CHUNK_LENGTH];
    
    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        float current = signal[i];
        // Flag 1.0 if sign has changed, 0.0 otherwise
        if ((current > 0 && last_sample <= 0) || (current < 0 && last_sample >= 0)) {
            out_buffer[i] = 1.0f;
        } else {
            out_buffer[i] = 0.0f;
        }
        last_sample = current;
    }
    
    return out_buffer;
}
