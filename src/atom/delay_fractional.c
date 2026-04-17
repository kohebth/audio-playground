#include <atom/dsp_atoms.h>
#include <stdlib.h>
#include <math.h>

#define CHUNK_LENGTH 512
#define MAX_DELAY_SAMPLES 192000

Signal delay_fractional(Signal signal, FractionalDelayParams params) {
    if (signal == NULL) return NULL;
    
    static float *delay_buffer = NULL;
    static uint32_t write_idx = 0;
    static float out_buffer[CHUNK_LENGTH];
    
    if (delay_buffer == NULL) {
        delay_buffer = (float *)calloc(MAX_DELAY_SAMPLES, sizeof(float));
    }
    
    float delay_samples = params.delay_samples;
    if (delay_samples > MAX_DELAY_SAMPLES - 1) delay_samples = MAX_DELAY_SAMPLES - 1;
    if (delay_samples < 0) delay_samples = 0;
    
    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        // Linear Interpolation
        float read_pos = (float)write_idx - delay_samples;
        if (read_pos < 0) read_pos += MAX_DELAY_SAMPLES;
        
        uint32_t idx_a = (uint32_t)floorf(read_pos);
        uint32_t idx_b = (idx_a + 1) % MAX_DELAY_SAMPLES;
        float frac = read_pos - floorf(read_pos);
        
        out_buffer[i] = delay_buffer[idx_a] * (1.0f - frac) + delay_buffer[idx_b] * frac;
        
        // Write new sample
        delay_buffer[write_idx] = signal[i];
        write_idx = (write_idx + 1) % MAX_DELAY_SAMPLES;
    }
    
    return out_buffer;
}
