#include <atom/dsp_atoms.h>
#include <stdlib.h>

#define CHUNK_LENGTH 512
#define MAX_ALLPASS_DELAY 48000

Signal filter_allpass(Signal signal, CombParams params) {
    if (signal == NULL) return NULL;
    
    static float *delay_buffer = NULL;
    static uint32_t write_idx = 0;
    static float out_buffer[CHUNK_LENGTH];
    
    if (delay_buffer == NULL) {
        delay_buffer = (float *)calloc(MAX_ALLPASS_DELAY, sizeof(float));
    }
    
    uint32_t delay_samples = (uint32_t)params.delay_samples;
    if (delay_samples > MAX_ALLPASS_DELAY) delay_samples = MAX_ALLPASS_DELAY;
    
    float g = params.coefficient;
    
    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        int32_t read_idx = (int32_t)write_idx - (int32_t)delay_samples;
        if (read_idx < 0) read_idx += MAX_ALLPASS_DELAY;
        
        float v_n = delay_buffer[read_idx]; // y[n-D]
        float y_n = -g * signal[i] + v_n;
        
        out_buffer[i] = y_n;
        delay_buffer[write_idx] = signal[i] + g * y_n; // Canonical form storage
        
        write_idx = (write_idx + 1) % MAX_ALLPASS_DELAY;
    }
    
    return out_buffer;
}
