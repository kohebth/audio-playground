#include <atom/dsp_atoms.h>
#include <math.h>

#define CHUNK_LENGTH 512

Signal *mix_decode_ms(Signal signal_m, Signal signal_s) {
    if (signal_m == NULL || signal_s == NULL) return NULL;
    
    static float left_data[CHUNK_LENGTH];
    static float right_data[CHUNK_LENGTH];
    static Signal output_pointers[2];
    static int initialized = 0;
    
    if (!initialized) {
        output_pointers[0] = left_data;
        output_pointers[1] = right_data;
        initialized = 1;
    }
    
    float inv_sqrt2 = (float)M_SQRT1_2;
    
    for (int i = 0; i < CHUNK_LENGTH; i++) {
        output_pointers[0][i] = (signal_m[i] + signal_s[i]) * inv_sqrt2;
        output_pointers[1][i] = (signal_m[i] - signal_s[i]) * inv_sqrt2;
    }
    
    return output_pointers;
}
