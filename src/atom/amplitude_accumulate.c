#include <atom/dsp_atoms.h>

#define CHUNK_LENGTH 512

Signal amplitude_accumulate(Signal signal) {
    if (signal == NULL) return NULL;
    
    float sum = 0.0f;
    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        sum += signal[i];
        signal[i] = sum;
    }
    
    return signal;
}
