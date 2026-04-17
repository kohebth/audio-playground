#include <atom/dsp_atoms.h>
#include <math.h>

#define CHUNK_LENGTH 512
#define TABLE_SIZE 1024

Signal nonlinear_waveshape(Signal signal, WaveshapeParams params) {
    if (signal == NULL || params.transfer_table == NULL) return signal;
    
    static float out_buffer[CHUNK_LENGTH];
    
    for (int i = 0; i < CHUNK_LENGTH; i++) {
        // Map [-1.0, 1.0] to [0, TABLE_SIZE - 1]
        float x = signal[i];
        float pos = (x + 1.0f) * 0.5f * (TABLE_SIZE - 1);
        
        if (pos < 0) pos = 0;
        if (pos > TABLE_SIZE - 2) pos = TABLE_SIZE - 2;
        
        uint32_t idx_a = (uint32_t)floorf(pos);
        uint32_t idx_b = idx_a + 1;
        float frac = pos - floorf(pos);
        
        // Linear interpolation in the transfer table
        out_buffer[i] = params.transfer_table[idx_a] * (1.0f - frac) + params.transfer_table[idx_b] * frac;
    }
    
    return out_buffer;
}
