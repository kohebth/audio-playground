#include <atom/dsp_atoms.h>

#define CHUNK_LENGTH 512

Signal mix_wet_dry(Signal dry, Signal wet, WetDryParams params) {
    if (dry == NULL || wet == NULL) return dry;
    
    static float out_buffer[CHUNK_LENGTH];
    float mix = params.mix;
    if (mix < 0.0f) mix = 0.0f;
    if (mix > 1.0f) mix = 1.0f;
    
    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        out_buffer[i] = (1.0f - mix) * dry[i] + mix * wet[i];
    }
    
    return out_buffer;
}
