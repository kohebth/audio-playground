#include <atom/dsp_atoms.h>

#define CHUNK_LENGTH 512

Signal generation_dc(float value) {
    static float buffer[CHUNK_LENGTH];
    
    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        buffer[i] = value;
    }
    
    return buffer;
}
