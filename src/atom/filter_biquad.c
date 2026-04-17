#include <atom/dsp_atoms.h>

#define CHUNK_LENGTH 512

Signal filter_biquad(Signal signal, BiquadParams params) {
    if (signal == NULL) return NULL;
    
    static float x1 = 0.0f, x2 = 0.0f, y1 = 0.0f, y2 = 0.0f;
    static float out_buffer[CHUNK_LENGTH];
    
    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        float x0 = signal[i];
        float y0 = params.b0 * x0 + params.b1 * x1 + params.b2 * x2 
                                 - params.a1 * y1 - params.a2 * y2;
        
        out_buffer[i] = y0;
        
        x2 = x1;
        x1 = x0;
        y2 = y1;
        y1 = y0;
    }
    
    return out_buffer;
}
