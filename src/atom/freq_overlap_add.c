#include <atom/dsp_atoms.h>
#include <string.h>
#include <stdlib.h>

#define MAX_OVERLAP_WINDOW 8192

Signal freq_overlap_add(OverlapAddParams params) {
    if (params.frames == NULL) return NULL;
    
    static float overlap_buffer[MAX_OVERLAP_WINDOW] = {0};
    static float out_signal[512]; // CHUNK_LENGTH
    
    uint32_t N = params.block_size;
    uint32_t H = params.hop_size;
    if (N > MAX_OVERLAP_WINDOW) N = MAX_OVERLAP_WINDOW;
    
    // For each frame (we assume 1 frame is provided per call for now, or we iterate)
    // Actually, OverlapAddParams usually refers to a single frame being added
    Buffer frame = params.frames[0]; 
    
    for (uint32_t i = 0; i < N; i++) {
        overlap_buffer[i] += frame[i];
    }
    
    // Output one hop size of data
    for (uint32_t i = 0; i < H; i++) {
        out_signal[i] = overlap_buffer[i];
    }
    
    // Shift the overlap buffer
    memmove(overlap_buffer, overlap_buffer + H, (MAX_OVERLAP_WINDOW - H) * sizeof(float));
    memset(overlap_buffer + (MAX_OVERLAP_WINDOW - H), 0, H * sizeof(float));
    
    return out_signal;
}
