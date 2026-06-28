#include <atom/dsp_atoms.h>
#include <stddef.h>

void mix_pan_stereo_process(
    mix_pan_stereo_out_t     *out,
    mix_pan_stereo_in_t      *in,
    mix_pan_stereo_params_t  *params,
    mix_pan_stereo_state_t   *state,
    const apg_process_info_t *info
) {
    if (out->left == NULL || out->right == NULL || in->signal == NULL)
        return;

    const uint32_t frames = apg_process_frames_or_default(info);
    float          p      = params->position;
    if (p < 0.0f)
        p = 0.0f;
    if (p > 1.0f)
        p = 1.0f;

    float g_l = 1.0f - p;
    float g_r = p;

    for (uint32_t i = 0; i < frames; ++i) {
        out->left[i]  = in->signal[i] * g_l;
        out->right[i] = in->signal[i] * g_r;
    }
}

void mix_pan_stereo(
    mix_pan_stereo_out_t *out, mix_pan_stereo_in_t *in, mix_pan_stereo_params_t *params, mix_pan_stereo_state_t *state
) {
    const apg_process_info_t info = apg_process_info_default();
    mix_pan_stereo_process(out, in, params, state, &info);
}
