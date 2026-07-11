#include <apgcore/dsp/dsp_safety.h>
#include <atom/dsp_atoms.h>
#include <math.h>
#include <stddef.h>

void detect_peak_process(
    detect_peak_out_t        *out,
    detect_peak_in_t         *in,
    detect_peak_params_t     *params,
    detect_peak_state_t      *state,
    const apg_process_info_t *info
) {
    if (out == NULL || in == NULL || params == NULL || state == NULL)
        return;
    if (out == NULL || in == NULL || out->level == NULL || in->signal == NULL || params == NULL || state == NULL)
        return;

    const float sample_rate   = apg_sample_rate_or_default(info);
    const float attack        = apg_clamp_float(params->attack, 0.0f, 60.0f);
    const float release       = apg_clamp_float(params->release, 0.0f, 60.0f);
    float       peak          = isfinite(state->prev_peak) ? state->prev_peak : 0.0f;
    const float attack_coeff  = expf(-1.0f / (attack * sample_rate + 1.0f));
    const float release_coeff = expf(-1.0f / (release * sample_rate + 1.0f));

    const uint32_t frames = apg_process_frames_or_default(info);
    for (uint32_t i = 0; i < frames; ++i) {
        const float sample = isfinite(in->signal[i]) ? in->signal[i] : 0.0f;
        const float abs_x  = fabsf(sample);
        if (abs_x > peak)
            peak = abs_x + attack_coeff * (peak - abs_x);
        else
            peak = abs_x + release_coeff * (peak - abs_x);
        peak          = apg_denormal_kill(peak);
        out->level[i] = peak;
    }
    state->prev_peak = peak;
}

void detect_peak(
    detect_peak_out_t *out, detect_peak_in_t *in, detect_peak_params_t *params, detect_peak_state_t *state
) {
    apg_process_info_t info = apg_process_info_default();
    detect_peak_process(out, in, params, state, &info);
}
