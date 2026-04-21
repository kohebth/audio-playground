#include <atom/dsp_atoms.h>
#include <math.h>

#define CHUNK_LENGTH 512

void nonlinear_samplerate_reduce(nonlinear_samplerate_reduce_out_t out, nonlinear_samplerate_reduce_in_t in, nonlinear_samplerate_reduce_params_t params, nonlinear_samplerate_reduce_state_t *state) {
    if (out.signal == NULL || in.signal == NULL || state == NULL) return;

    float factor = params.factor;
    if (factor < 1.0f) factor = 1.0f;
    float last_val = state->last_val;
    float counter = state->counter;

    for (int i = 0; i < CHUNK_LENGTH; i++) {
        if (counter >= factor) {
            last_val = in.signal[i];
            counter -= factor;
        }
        out.signal[i] = last_val;
        counter += 1.0f;
    }

    state->last_val = last_val;
    state->counter = counter;
}
