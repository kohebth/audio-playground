#include <atom/dsp_atoms.h>

#define CHUNK_LENGTH 512

Signal generation_envelope(EnvelopeParams params) {
    static float buffer[CHUNK_LENGTH];
    static float current_level = 0.0f;
    static enum { ATTACK, DECAY, SUSTAIN, RELEASE, IDLE } state = IDLE;
    
    float attack_step = 1.0f / (params.attack * params.sample_rate);
    float decay_step = (1.0f - params.sustain) / (params.decay * params.sample_rate);
    float release_step = params.sustain / (params.release * params.sample_rate);
    
    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        if (params.gate > 0.5f) {
            if (state == IDLE || state == RELEASE) state = ATTACK;
            
            if (state == ATTACK) {
                current_level += attack_step;
                if (current_level >= 1.0f) {
                    current_level = 1.0f;
                    state = DECAY;
                }
            } else if (state == DECAY) {
                current_level -= decay_step;
                if (current_level <= params.sustain) {
                    current_level = params.sustain;
                    state = SUSTAIN;
                }
            } else if (state == SUSTAIN) {
                current_level = params.sustain;
            }
        } else {
            if (state != IDLE) state = RELEASE;
            
            if (state == RELEASE) {
                current_level -= release_step;
                if (current_level <= 0.0f) {
                    current_level = 0.0f;
                    state = IDLE;
                }
            }
        }
        buffer[i] = current_level;
    }
    
    return buffer;
}
