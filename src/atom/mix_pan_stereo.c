#include <atom/dsp_atoms.h>

#define CHUNK_LENGTH 512

Signal *mix_pan_stereo(Signal signal, PanParams params) {
    if (signal == NULL) return NULL;
    
    static float left_data[CHUNK_LENGTH];
    static float right_data[CHUNK_LENGTH];
    static Signal output_pointers[2];
    static int initialized = 0;
    
    if (!initialized) {
        output_pointers[0] = left_data;
        output_pointers[1] = right_data;
        initialized = 1;
    }
    
    float p = params.position; // 0.0 for left, 1.0 for right, 0.5 central
    if (p < 0.0f) p = 0.0f;
    if (p > 1.0f) p = 1.0f;
    
    float g_l = 1.0f - p;
    float g_r = p;
    
    for (int i = 0; i < CHUNK_LENGTH; i++) {
        output_pointers[0][i] = signal[i] * g_l;
        output_pointers[1][i] = signal[i] * g_r;
    }
    
    return output_pointers;
}
