#include <atom/dsp_atoms.h>
#include <math.h>

#define CHUNK_LENGTH 512

Signal *mix_encode_ms(Signal signal_l, Signal signal_r) {
    if (signal_l == NULL || signal_r == NULL) return NULL;
    
    static float mid_data[CHUNK_LENGTH];
    static float side_data[CHUNK_LENGTH];
    static Signal output_pointers[2];
    static int initialized = 0;
    
    if (!initialized) {
        output_pointers[0] = mid_data;
        output_pointers[1] = side_data;
        initialized = 1;
    }
    
    float inv_sqrt2 = (float)M_SQRT1_2;
    
    for (int i = 0; i < CHUNK_LENGTH; i++) {
        output_pointers[0][i] = (signal_l[i] + signal_r[i]) * inv_sqrt2;
        output_pointers[1][i] = (signal_l[i] - signal_r[i]) * inv_sqrt2;
    }
    
    return output_pointers;
}
