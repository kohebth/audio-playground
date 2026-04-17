#include <atom/dsp_atoms.h>
#include <stdlib.h>

#define CHUNK_LENGTH 512
#define MAX_COMB_DELAY 48000

Signal filter_comb_fb(Signal signal, CombParams params) {
    if (signal == NULL) return NULL;
    
    static float *delay_buffer = NULL;
    static uint32_t write_idx = 0;
    static float out_buffer[CHUNK_LENGTH];
    
    if (delay_buffer == NULL) {
        delay_buffer = (float *)calloc(MAX_COMB_DELAY, sizeof(float));
    }
    
    uint32_t delay_samples = (uint32_t)params.delay_samples;
    if (delay_samples > MAX_COMB_DELAY) delay_samples = MAX_COMB_DELAY;
    
    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        int32_t read_idx = (int32_t)write_idx - (int32_t)delay_samples;
        if (read_idx < 0) read_idx += MAX_COMB_DELAY;
        
        float delayed = delay_buffer[read_idx];
        float out = signal[i] + params.coefficient * delayed;
        
        out_buffer[i] = out;
        delay_buffer[write_idx] = out; // Feedback is written to buffer
        write_idx = (write_idx + 1) % MAX_COMB_DELAY;
    }
    
    return out_buffer;
}
