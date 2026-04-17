#include <atom/dsp_atoms.h>

#define CHUNK_LENGTH 512

Signal filter_dc_block(Signal signal, DcBlockParams params) {
    if (signal == NULL) return NULL;
    
    static float x1 = 0.0f;
    static float y1 = 0.0f;
    static float out_buffer[CHUNK_LENGTH];
    
    float R = params.coefficient;
    
    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        float x0 = signal[i];
        float y0 = x0 - x1 + R * y1;
        
        out_buffer[i] = y0;
        
        x1 = x0;
        y1 = y0;
    }
    
    return out_buffer;
}
