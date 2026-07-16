#include <apgcore/dsp/dsp_safety.h>
#include <atom/dsp_atoms.h>
#include <math.h>
#include <stddef.h>

void detect_envelope_process(
    detect_envelope_out_t          *out,
    const detect_envelope_in_t     *in,
    const detect_envelope_params_t *params,
    detect_envelope_state_t        *state,
    const apg_process_context_t    *info
) {
    if (!apg_process_context_valid(info))
        return;
    if (out == NULL || in == NULL || params == NULL || state == NULL)
        return;
    if (out == NULL || in == NULL || out->envelope == NULL || in->signal == NULL || params == NULL || state == NULL)
        return;

    const float sample_rate   = apg_process_context_sample_rate(info);
    const float attack        = apg_clamp_float(params->attack, 0.0f, 60.0f);
    const float release       = apg_clamp_float(params->release, 0.0f, 60.0f);
    float       env           = isfinite(state->prev_envelope) ? state->prev_envelope : 0.0f;
    const float attack_coeff  = expf(-1.0f / (attack * sample_rate + 1.0f));
    const float release_coeff = expf(-1.0f / (release * sample_rate + 1.0f));

    const uint32_t frames = apg_process_context_frames(info);
    for (uint32_t i = 0; i < frames; ++i) {
        const float sample = isfinite(in->signal[i]) ? in->signal[i] : 0.0f;
        const float abs_x  = fabsf(sample);
        if (abs_x > env)
            env = abs_x + attack_coeff * (env - abs_x);
        else
            env = abs_x + release_coeff * (env - abs_x);
        env              = apg_denormal_kill(env);
        out->envelope[i] = env;
    }
    state->prev_envelope = env;
}
