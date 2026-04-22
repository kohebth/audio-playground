#include <atom/dsp_atoms.h>
#include <stddef.h>

#define CHUNK_LENGTH 512

void mix_crossfade(
    mix_crossfade_out_t *out, mix_crossfade_in_t *in, mix_crossfade_params_t *params, mix_crossfade_state_t *state
) {
    if (out->signal == NULL || in->signal_a == NULL || in->signal_b == NULL)
        return;

    float t = params->t;
    if (t < 0.0f)
        t = 0.0f;
    if (t > 1.0f)
        t = 1.0f;

    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        out->signal[i] = (1.0f - t) * in->signal_a[i] + t * in->signal_b[i];
    }
}
