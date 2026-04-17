#include <atom/dsp_atoms.h>
#include <fast_math.h>

#define CHUNK_LENGTH 512

Signal amplitude_clip_hard(Signal signal, ClipParams params) {
    if (signal == NULL) return NULL;
    
    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        if (signal[i] > params.threshold) signal[i] = params.threshold;
        else if (signal[i] < -params.threshold) signal[i] = -params.threshold;
    }
    
    return signal;
}
