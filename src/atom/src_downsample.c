#include <atom/dsp_atoms.h>

#define CHUNK_LENGTH 512

Signal src_downsample(Signal signal, SrcParams params) {
    if (signal == NULL) return NULL;
    
    static float out_buffer[CHUNK_LENGTH];
    uint32_t M = params.factor;
    if (M == 0) M = 1;
    
    for (int i = 0; i < CHUNK_LENGTH / M; ++i) {
        out_buffer[i] = signal[i * M];
    }
    
    return out_buffer;
}
