#include <atom/dsp_atoms.h>
#include <math.h>
#include <stddef.h>

void detect_envelope_process(
    detect_envelope_out_t    *out,
    detect_envelope_in_t     *in,
    detect_envelope_params_t *params,
    detect_envelope_state_t  *state,
    const apg_process_info_t *info
) {
    if (out->envelope == NULL || in->signal == NULL || params == NULL || state == NULL)
        return;

    float sample_rate   = params->sample_rate > 0.0f ? params->sample_rate
                                                     : (info && info->sample_rate > 0.0f ? info->sample_rate : 48000.0f);
    float env           = state->prev_envelope;
    float attack_coeff  = expf(-1.0f / (params->attack * sample_rate + 1.0f));
    float release_coeff = expf(-1.0f / (params->release * sample_rate + 1.0f));

    const uint32_t frames = apg_process_frames_or_default(info);
    for (uint32_t i = 0; i < frames; ++i) {
        float abs_x = fabsf(in->signal[i]);
        if (abs_x > env) {
            env = abs_x + attack_coeff * (env - abs_x);
        } else {
            env = abs_x + release_coeff * (env - abs_x);
        }
        out->envelope[i] = env;
    }
    state->prev_envelope = env;
}

void detect_envelope(
    detect_envelope_out_t    *out,
    detect_envelope_in_t     *in,
    detect_envelope_params_t *params,
    detect_envelope_state_t  *state
) {
    apg_process_info_t info = apg_process_info_default();
    detect_envelope_process(out, in, params, state, &info);
}
