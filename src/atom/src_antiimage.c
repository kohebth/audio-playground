#include <atom/dsp_atoms.h>
#include <math.h>

#define CHUNK_LENGTH 512

Signal src_antiimage(Signal signal, FilterParams params) {
    if (signal == NULL) return NULL;
    
    // Simple 2nd order Butterworth LPF for anti-imaging
    static float x1 = 0.0f, x2 = 0.0f, y1 = 0.0f, y2 = 0.0f;
    static float out_buffer[CHUNK_LENGTH];
    
    float ff = params.cutoff / params.sample_rate;
    float ita = 1.0f / tanf(M_PI * ff);
    float q = M_SQRT1_2;
    float b0 = 1.0f / (1.0f + ita / q + ita * ita);
    float b1 = 2.0f * b0;
    float b2 = b0;
    float a1 = 2.0f * (1.0f - ita * ita) * b0;
    float a2 = (1.0f - ita / q + ita * ita) * b0;
    
    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        float x0 = signal[i];
        float y0 = b0 * x0 + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        
        out_buffer[i] = y0;
        
        x2 = x1; x1 = x0;
        y2 = y1; y1 = y0;
    }
    
    return out_buffer;
}
