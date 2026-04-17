#include <atom/dsp_atoms.h>
#include <stdlib.h>
#include <math.h>

#define CHUNK_LENGTH 512
#define MAX_MOD_DELAY 4096

Signal modulation_phase(Signal signal, ModulationParams params) {
    if (signal == NULL || params.modulator == NULL) return signal;
    
    static float *delay_buffer = NULL;
    static uint32_t write_idx = 0;
    static float out_buffer[CHUNK_LENGTH];
    
    if (delay_buffer == NULL) {
        delay_buffer = (float *)calloc(MAX_MOD_DELAY, sizeof(float));
    }
    
    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        // Phase modulation via delay
        float delay_samples = (1.0f + params.modulator[i] * params.depth);
        if (delay_samples < 0) delay_samples = 0;
        if (delay_samples > MAX_MOD_DELAY - 1) delay_samples = MAX_MOD_DELAY - 1;
        
        float read_pos = (float)write_idx - delay_samples;
        if (read_pos < 0) read_pos += MAX_MOD_DELAY;
        
        uint32_t idx_a = (uint32_t)floorf(read_pos);
        uint32_t idx_b = (idx_a + 1) % MAX_MOD_DELAY;
        float frac = read_pos - floorf(read_pos);
        
        out_buffer[i] = delay_buffer[idx_a] * (1.0f - frac) + delay_buffer[idx_b] * frac;
        
        delay_buffer[write_idx] = signal[i];
        write_idx = (write_idx + 1) % MAX_MOD_DELAY;
    }
    
    return out_buffer;
}
