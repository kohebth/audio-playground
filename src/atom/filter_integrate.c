#include <atom/dsp_atoms.h>

#define CHUNK_LENGTH 512

Signal filter_integrate(Signal signal) {
    if (signal == NULL) return NULL;
    
    static float last_y = 0.0f;
    static float out_buffer[CHUNK_LENGTH];
    
    // Leaky integrator to prevent DC build-up/overflow
    const float leakage = 0.999f;
    
    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        float y0 = signal[i] + leakage * last_y;
        out_buffer[i] = y0;
        last_y = y0;
    }
    
    return out_buffer;
}
