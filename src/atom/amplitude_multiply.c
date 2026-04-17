#include <atom/dsp_atoms.h>

#define CHUNK_LENGTH 512

Signal amplitude_multiply(Signal signal, float scalar) {
    if (signal == NULL) return NULL;
    
    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        signal[i] *= scalar;
    }
    
    return signal;
}
