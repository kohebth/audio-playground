#include <atom/dsp_atoms.h>
#include <math.h>

#define CHUNK_LENGTH 512

Signal amplitude_smooth(Signal signal, SmoothParams params) {
    if (signal == NULL) return NULL;
    
    static float last_out = 0.0f;
    float alpha = 1.0f - expf(-1.0f / (params.time * params.sample_rate));
    
    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        last_out += alpha * (signal[i] - last_out);
        signal[i] = last_out;
    }
    
    return signal;
}
