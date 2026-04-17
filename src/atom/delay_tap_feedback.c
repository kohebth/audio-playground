#include <atom/dsp_atoms.h>

#define CHUNK_LENGTH 512

Signal delay_tap_feedback(Buffer delay_line, TapParams params) {
    if (delay_line == NULL) return NULL;
    
    static float out_buffer[CHUNK_LENGTH];
    uint32_t tap_idx = (uint32_t)params.tap_position;
    
    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        // This is a simplified implementation. 
        // Real circular taps require a write head which is not provided in the signature.
        // We assume delay_line is large enough and we are accessing it with some external knowledge.
        out_buffer[i] = delay_line[tap_idx + i] * params.coefficient;
    }
    
    return out_buffer;
}
