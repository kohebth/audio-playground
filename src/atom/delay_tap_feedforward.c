#include <atom/dsp_atoms.h>

#define CHUNK_LENGTH 512

Signal delay_tap_feedforward(Buffer delay_line, TapParams params) {
    if (delay_line == NULL) return NULL;
    
    static float out_buffer[CHUNK_LENGTH];
    uint32_t tap_idx = (uint32_t)params.tap_position;
    
    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        out_buffer[i] = delay_line[tap_idx + i] * params.coefficient;
    }
    
    return out_buffer;
}
