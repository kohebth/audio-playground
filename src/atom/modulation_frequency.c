#include <atom/dsp_atoms.h>
#include <stdlib.h>
#include <math.h>

#define CHUNK_LENGTH 512
#define MAX_FM_DELAY 4096

Signal modulation_frequency(Signal signal, ModulationParams params) {
    if (signal == NULL || params.modulator == NULL) return signal;
    
    static float *delay_buffer = NULL;
    static float current_delay = 0.0f;
    static uint32_t write_idx = 0;
    static float out_buffer[CHUNK_LENGTH];
    
    if (delay_buffer == NULL) {
        delay_buffer = (float *)calloc(MAX_FM_DELAY, sizeof(float));
    }
    
    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        // Integrate frequency modulation signal to get delay position (phase)
        current_delay += params.modulator[i] * params.depth;
        
        // Clamp current_delay to safe range
        if (current_delay < 0) current_delay = 0;
        if (current_delay > MAX_FM_DELAY - 1) current_delay = MAX_FM_DELAY - 1;
        
        float read_pos = (float)write_idx - current_delay;
        if (read_pos < 0) read_pos += MAX_FM_DELAY;
        
        uint32_t idx_a = (uint32_t)floorf(read_pos);
        uint32_t idx_b = (idx_a + 1) % MAX_FM_DELAY;
        float frac = read_pos - floorf(read_pos);
        
        out_buffer[i] = delay_buffer[idx_a] * (1.0f - frac) + delay_buffer[idx_b] * frac;
        
        delay_buffer[write_idx] = signal[i];
        write_idx = (write_idx + 1) % MAX_FM_DELAY;
    }
    
    return out_buffer;
}
