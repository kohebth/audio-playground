#include <atom/dsp_atoms.h>

#define CHUNK_LENGTH 512

Signal amplitude_divide(float numerator, Signal denominator) {
    if (denominator == NULL) return NULL;
    
    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        if (denominator[i] != 0.0f) {
            denominator[i] = numerator / denominator[i];
        } else {
            denominator[i] = 0.0f; // Prevent division by zero
        }
    }
    
    return denominator;
}
