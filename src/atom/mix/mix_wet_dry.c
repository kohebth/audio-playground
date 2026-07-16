#include <atom/dsp_atoms.h>
#include <stddef.h>

void mix_wet_dry_process(
    mix_wet_dry_out_t           *out,
    const mix_wet_dry_in_t      *in,
    const mix_wet_dry_params_t  *params,
    mix_wet_dry_state_t         *state,
    const apg_process_context_t *info
) {
    if (!apg_process_context_valid(info))
        return;
    if (out == NULL || in == NULL || params == NULL || state == NULL)
        return;
    (void)state;
    if (out->signal == NULL || in->dry == NULL || in->wet == NULL || params == NULL)
        return;

    float mix = params->mix;
    if (mix < 0.0f)
        mix = 0.0f;
    if (mix > 1.0f)
        mix = 1.0f;

    const uint32_t frames = apg_process_context_frames(info);
    for (uint32_t i = 0; i < frames; ++i) {
        out->signal[i] = (1.0f - mix) * in->dry[i] + mix * in->wet[i];
    }
}
