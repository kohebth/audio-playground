#include <atom/dsp_atoms.h>
#include <math.h>

#define CHUNK_LENGTH 512

Signal nonlinear_bitcrush(Signal signal, BitcrushParams params) {
    if (signal == NULL) return NULL;
    
    static float out_buffer[CHUNK_LENGTH];
    float levels = powf(2.0f, params.bit_depth);
    
    for (int i = 0; i < CHUNK_LENGTH; i++) {
        // Quantize signal
        out_buffer[i] = roundf(signal[i] * levels) / levels;
    }
    
    return out_buffer;
}
