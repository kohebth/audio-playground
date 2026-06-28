#include <atom/dsp_atoms.h>
#include <stddef.h>

void mix_wet_dry_process(
    mix_wet_dry_out_t        *out,
    mix_wet_dry_in_t         *in,
    mix_wet_dry_params_t     *params,
    mix_wet_dry_state_t      *state,
    const apg_process_info_t *info
) {
    (void)state;
    if (out->signal == NULL || in->dry == NULL || in->wet == NULL || params == NULL)
        return;

    float mix = params->mix;
    if (mix < 0.0f)
        mix = 0.0f;
    if (mix > 1.0f)
        mix = 1.0f;

    const uint32_t frames = apg_process_frames_or_default(info);
    for (uint32_t i = 0; i < frames; ++i) {
        out->signal[i] = (1.0f - mix) * in->dry[i] + mix * in->wet[i];
    }
}

void mix_wet_dry(
    mix_wet_dry_out_t *out, mix_wet_dry_in_t *in, mix_wet_dry_params_t *params, mix_wet_dry_state_t *state
) {
    apg_process_info_t info = apg_process_info_default();
    mix_wet_dry_process(out, in, params, state, &info);
}
