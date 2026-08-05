#include <apgcore/dsp/dsp_safety.h>
#include <atom/dsp_atoms.h>
#include <math.h>
#include <stddef.h>

void amplitude_smooth_process(
    amplitude_smooth_out_t          *out,
    const amplitude_smooth_in_t     *in,
    const amplitude_smooth_params_t *params,
    amplitude_smooth_state_t        *state,
    const apg_process_context_t     *info
) {
    if (!apg_process_context_valid(info))
        return;
    if (out == NULL || in == NULL || params == NULL || state == NULL)
        return;
    if (out == NULL || in == NULL || out->signal == NULL || in->signal == NULL || params == NULL || state == NULL)
        return;

    const float sample_rate = apg_process_context_sample_rate(info);
    const float attack      = apg_clamp_float(params->attack, 0.0f, 60.0f);
    const float release     = apg_clamp_float(params->release, 0.0f, 60.0f);
    const float alpha_att   = 1.0f - expf(-1.0f / (attack * sample_rate + 1.0f));
    const float alpha_rel   = 1.0f - expf(-1.0f / (release * sample_rate + 1.0f));
    float       last_out    = isfinite(state->prev_value) ? state->prev_value : 0.0f;

    const uint32_t frames = apg_process_context_frames(info);
    for (uint32_t i = 0; i < frames; ++i) {
        const float input = isfinite(in->signal[i]) ? in->signal[i] : 0.0f;
        const float alpha = input > last_out ? alpha_att : alpha_rel;
        last_out += alpha * (input - last_out);
        last_out       = apg_denormal_kill(last_out);
        out->signal[i] = last_out;
    }

    state->prev_value = last_out;
}
