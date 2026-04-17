#include <atom/dsp_atoms.h>
#include <stdint.h>

#define CHUNK_LENGTH 512

Signal generation_impulse(uint32_t interval, uint32_t sample_rate) {
    static float buffer[CHUNK_LENGTH];
    static uint32_t counter = 0;
    
    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        if (counter == 0) {
            buffer[i] = 1.0f;
            counter = interval;
        } else {
            buffer[i] = 0.0f;
            counter--;
        }
    }
    
    return buffer;
}
