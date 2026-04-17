#include <atom/dsp_atoms.h>
#include <string.h>
#include <stdlib.h>

#define MAX_FRAMES 16
#define MAX_FRAME_SIZE 1024

Buffer *freq_overlap_save(OverlapSaveParams params) {
    if (params.signal == NULL) return NULL;
    
    static float frames_data[MAX_FRAMES][MAX_FRAME_SIZE];
    static Buffer frame_pointers[MAX_FRAMES];
    static int initialized = 0;
    
    if (!initialized) {
        for (int i = 0; i < MAX_FRAMES; i++) {
            frame_pointers[i] = frames_data[i];
        }
        initialized = 1;
    }
    
    uint32_t N = params.block_size;
    uint32_t H = params.hop_size;
    if (N > MAX_FRAME_SIZE) N = MAX_FRAME_SIZE;
    
    // Simplification: In a real system, this would maintain a circular buffer
    // and return N-length frames every H samples.
    // For this atom, we'll just fill the first few frames with the input signal chunks
    // assuming the input signal is large enough.
    
    for (int f = 0; f < 2; f++) { // Example: return 2 frames
        memcpy(frame_pointers[f], params.signal + (f * H), N * sizeof(float));
    }
    
    return frame_pointers;
}
