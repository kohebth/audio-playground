#include <atom/dsp_atoms.h>

#define CHUNK_LENGTH 512

Signal modulation_amplitude(Signal signal, ModulationParams params) {
    if (signal == NULL || params.modulator == NULL) return signal;
    
    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        signal[i] *= (1.0f + params.depth * params.modulator[i]);
    }
    
    return signal;
}
