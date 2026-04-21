#include <atom/dsp_atoms.h>
#include <math.h>

#define CHUNK_LENGTH 512

void detect_envelope(detect_envelope_out_t out, detect_envelope_in_t in, detect_envelope_params_t params, detect_envelope_state_t *state) {
    if (out.envelope == NULL || in.signal == NULL || state == NULL) return;

    float env = state->prev_envelope;
    float attack_coeff = expf(-1.0f / (params.attack * params.sample_rate + 1.0f));
    float release_coeff = expf(-1.0f / (params.release * params.sample_rate + 1.0f));

    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        float abs_x = fabsf(in.signal[i]);
        if (abs_x > env) {
            env = abs_x + attack_coeff * (env - abs_x);
        } else {
            env = abs_x + release_coeff * (env - abs_x);
        }
        out.envelope[i] = env;
    }
    state->prev_envelope = env;
}
