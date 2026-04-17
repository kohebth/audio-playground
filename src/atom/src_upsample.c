#include <atom/dsp_atoms.h>
#include <stdlib.h>
#include <string.h>

#define CHUNK_LENGTH 512
#define MAX_UPSAMPLE_FACTOR 8

Signal src_upsample(Signal signal, SrcParams params) {
    if (signal == NULL) return NULL;
    
    static float out_buffer[CHUNK_LENGTH * MAX_UPSAMPLE_FACTOR];
    uint32_t L = params.factor;
    if (L > MAX_UPSAMPLE_FACTOR) L = MAX_UPSAMPLE_FACTOR;
    if (L == 0) L = 1;
    
    memset(out_buffer, 0, sizeof(out_buffer));
    
    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        out_buffer[i * L] = signal[i];
    }
    
    return out_buffer;
}
