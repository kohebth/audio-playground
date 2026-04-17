#include <atom/dsp_atoms.h>
#include <math.h>

#define CHUNK_LENGTH 512

Signal modulation_scrub(Buffer buffer, ScrubParams params) {
    if (buffer == NULL || params.position_signal == NULL) return NULL;
    
    static float out_buffer[CHUNK_LENGTH];
    
    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        float pos = params.position_signal[i];
        
        uint32_t idx_a = (uint32_t)floorf(pos);
        uint32_t idx_b = idx_a + 1;
        float frac = pos - floorf(pos);
        
        // Linear interpolation from the source buffer
        out_buffer[i] = buffer[idx_a] * (1.0f - frac) + buffer[idx_b] * frac;
    }
    
    return out_buffer;
}
