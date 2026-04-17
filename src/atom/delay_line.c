#include <atom/dsp_atoms.h>
#include <stdlib.h>
#include <string.h>

#define CHUNK_LENGTH 512
#define MAX_DELAY_SAMPLES 192000 // 4 seconds at 48kHz

Signal delay_line(Signal signal, DelayLineParams params) {
    if (signal == NULL) return NULL;
    
    static float *delay_buffer = NULL;
    static uint32_t write_idx = 0;
    static float out_buffer[CHUNK_LENGTH];
    
    if (delay_buffer == NULL) {
        delay_buffer = (float *)calloc(MAX_DELAY_SAMPLES, sizeof(float));
    }
    
    uint32_t delay_samples = params.length;
    if (delay_samples > MAX_DELAY_SAMPLES) delay_samples = MAX_DELAY_SAMPLES;
    
    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        // Read delayed sample
        int32_t read_idx = (int32_t)write_idx - (int32_t)delay_samples;
        if (read_idx < 0) read_idx += MAX_DELAY_SAMPLES;
        
        out_buffer[i] = delay_buffer[read_idx];
        
        // Write new sample
        delay_buffer[write_idx] = signal[i];
        write_idx = (write_idx + 1) % MAX_DELAY_SAMPLES;
    }
    
    return out_buffer;
}
