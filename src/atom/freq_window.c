#include <atom/dsp_atoms.h>
#include <math.h>

#define CHUNK_LENGTH 512

Signal freq_window(Signal signal, WindowType window_type, uint32_t block_size) {
    if (signal == NULL) return NULL;
    
    uint32_t N = block_size;
    if (N == 0) N = CHUNK_LENGTH;
    
    for (uint32_t i = 0; i < N; ++i) {
        float w = 1.0f;
        float factor = (float)i / (float)(N - 1);
        
        switch (window_type) {
            case WINDOW_HANN:
                w = 0.5f * (1.0f - cosf(2.0f * M_PI * factor));
                break;
            case WINDOW_HAMMING:
                w = 0.54f - 0.46f * cosf(2.0f * M_PI * factor);
                break;
            case WINDOW_BLACKMAN:
                w = 0.42f - 0.5f * cosf(2.0f * M_PI * factor) + 0.08f * cosf(4.0f * M_PI * factor);
                break;
            case WINDOW_RECTANGULAR:
            default:
                w = 1.0f;
                break;
        }
        signal[i] *= w;
    }
    
    return signal;
}
