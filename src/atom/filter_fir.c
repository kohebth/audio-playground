#include <atom/dsp_atoms.h>
#include <stdlib.h>
#include <string.h>

#define CHUNK_LENGTH 512
#define MAX_FIR_SIZE 1024

Signal filter_fir(Signal signal, float *kernel, uint32_t kernel_size) {
    if (signal == NULL || kernel == NULL) return signal;
    
    static float history[MAX_FIR_SIZE];
    static uint32_t history_idx = 0;
    static float out_buffer[CHUNK_LENGTH];
    
    uint32_t k_size = kernel_size;
    if (k_size > MAX_FIR_SIZE) k_size = MAX_FIR_SIZE;
    
    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        // Update history
        history[history_idx] = signal[i];
        
        // FIR Convolution
        float acc = 0.0f;
        for (uint32_t k = 0; k < k_size; ++k) {
            int32_t idx = (int32_t)history_idx - (int32_t)k;
            if (idx < 0) idx += MAX_FIR_SIZE;
            acc += history[idx] * kernel[k];
        }
        
        out_buffer[i] = acc;
        history_idx = (history_idx + 1) % MAX_FIR_SIZE;
    }
    
    return out_buffer;
}
