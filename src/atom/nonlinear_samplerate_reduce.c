#include <atom/dsp_atoms.h>
#include <math.h>

#define CHUNK_LENGTH 512

Signal nonlinear_samplerate_reduce(Signal signal, SampleRateReduceParams params) {
    if (signal == NULL) return NULL;
    
    static float out_buffer[CHUNK_LENGTH];
    static float last_val = 0.0f;
    static float counter = 0.0f;
    
    float factor = params.reduction_factor;
    if (factor < 1.0f) factor = 1.0f;
    
    for (int i = 0; i < CHUNK_LENGTH; i++) {
        if (counter >= factor) {
            last_val = signal[i];
            counter -= factor;
        }
        out_buffer[i] = last_val;
        counter += 1.0f;
    }
    
    return out_buffer;
}
