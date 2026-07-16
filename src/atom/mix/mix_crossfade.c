#include <atom/dsp_atoms.h>
#include <stddef.h>

void mix_crossfade_process(
    mix_crossfade_out_t          *out,
    const mix_crossfade_in_t     *in,
    const mix_crossfade_params_t *params,
    mix_crossfade_state_t        *state,
    const apg_process_context_t  *info
) {
    if (!apg_process_context_valid(info))
        return;
    if (out == NULL || in == NULL || params == NULL || state == NULL)
        return;
    (void)state;
    if (out->signal == NULL || in->signal_a == NULL || in->signal_b == NULL || params == NULL)
        return;

    float t = params->t;
    if (t < 0.0f)
        t = 0.0f;
    if (t > 1.0f)
        t = 1.0f;

    const uint32_t frames = apg_process_context_frames(info);
    for (uint32_t i = 0; i < frames; ++i) {
        out->signal[i] = (1.0f - t) * in->signal_a[i] + t * in->signal_b[i];
    }
}
