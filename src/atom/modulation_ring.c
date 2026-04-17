#include <atom/dsp_atoms.h>

#define CHUNK_LENGTH 512

Signal modulation_ring(Signal signal, Buffer modulator) {
    if (signal == NULL || modulator == NULL) return signal;
    
    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        signal[i] *= modulator[i];
    }
    
    return signal;
}
