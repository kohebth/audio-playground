#include <atom/dsp_atoms.h>

#define CHUNK_LENGTH 512

Signal mix_crossfade(Signal signal_a, Signal signal_b, CrossfadeParams params) {
    if (signal_a == NULL || signal_b == NULL) return signal_a;
    
    static float out_buffer[CHUNK_LENGTH];
    float t = params.t;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    
    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        out_buffer[i] = (1.0f - t) * signal_a[i] + t * signal_b[i];
    }
    
    return out_buffer;
}
