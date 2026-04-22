#include <atom/dsp_atoms.h>
#include <stddef.h>

#define CHUNK_LENGTH 512

void mix_wet_dry(
    mix_wet_dry_out_t *out, mix_wet_dry_in_t *in, mix_wet_dry_params_t *params, mix_wet_dry_state_t *state
) {
    if (out->signal == NULL || in->dry == NULL || in->wet == NULL)
        return;

    float mix = params->mix;
    if (mix < 0.0f)
        mix = 0.0f;
    if (mix > 1.0f)
        mix = 1.0f;

    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        out->signal[i] = (1.0f - mix) * in->dry[i] + mix * in->wet[i];
    }
}
