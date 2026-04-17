#include <atom/dsp_atoms.h>
#include <math.h>
#include <stdlib.h>

#define CHUNK_LENGTH 512
#define MAX_RMS_WINDOW 4096

Signal detect_rms(Signal signal, RmsParams params) {
    if (signal == NULL) return NULL;
    
    static float window[MAX_RMS_WINDOW] = {0};
    static uint32_t write_idx = 0;
    static float sum_sq = 0.0f;
    static float out_buffer[CHUNK_LENGTH];
    
    uint32_t w_size = params.window_size;
    if (w_size > MAX_RMS_WINDOW) w_size = MAX_RMS_WINDOW;
    if (w_size == 0) w_size = 1;
    
    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        float x = signal[i];
        float x_sq = x * x;
        
        // Subtract old square, add new square
        sum_sq -= window[write_idx];
        window[write_idx] = x_sq;
        sum_sq += x_sq;
        
        write_idx = (write_idx + 1) % w_size;
        
        out_buffer[i] = sqrtf(fmaxf(0.0f, sum_sq / (float)w_size));
    }
    
    return out_buffer;
}
