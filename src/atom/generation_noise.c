#include <atom/dsp_atoms.h>
#include <stdlib.h>

#define CHUNK_LENGTH 512

Signal generation_noise(WaveformType color, uint32_t seed) {
    static float buffer[CHUNK_LENGTH];
    static uint32_t last_seed = 0;
    
    if (seed != 0) srand(seed);
    
    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        float white = ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f;
        
        switch (color) {
            case WAVEFORM_NOISE_WHITE:
                buffer[i] = white;
                break;
            case WAVEFORM_NOISE_PINK:
                // Simple Voss-McCartney approximation or similar could be here
                // For now, fall back to white with some filtering
                buffer[i] = white * 0.5f; 
                break;
            case WAVEFORM_NOISE_BROWN:
                // Simple integration
                {
                    static float brown_state = 0.0f;
                    brown_state += white * 0.1f;
                    if (brown_state > 1.0f) brown_state = 1.0f;
                    if (brown_state < -1.0f) brown_state = -1.0f;
                    buffer[i] = brown_state;
                }
                break;
            default:
                buffer[i] = white;
                break;
        }
    }
    
    return buffer;
}
