#include <atom/dsp_atoms.h>

#define CHUNK_LENGTH 512

Signal amplitude_subtract(Signal signal_a, Signal signal_b) {
    if (signal_a == NULL || signal_b == NULL) return signal_a;
    
    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        signal_a[i] -= signal_b[i];
    }
    
    return signal_a;
}
