#include <atom/dsp_atoms.h>
#include <math.h>

#define CHUNK_LENGTH 512

Signal detect_envelope(Signal signal, DetectorParams params) {
    if (signal == NULL) return NULL;
    
    static float envelope = 0.0f;
    static float out_buffer[CHUNK_LENGTH];
    
    float attack_coeff = expf(-1.0f / (params.attack * params.sample_rate));
    float release_coeff = expf(-1.0f / (params.release * params.sample_rate));
    
    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        float abs_x = fabsf(signal[i]);
        if (abs_x > envelope) {
            envelope = abs_x + attack_coeff * (envelope - abs_x);
        } else {
            envelope = abs_x + release_coeff * (envelope - abs_x);
        }
        out_buffer[i] = envelope;
    }
    
    return out_buffer;
}
