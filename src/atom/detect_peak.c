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
    if (out->level == NULL || in->signal == NULL || params == NULL || state == NULL)
        return;

    float sample_rate   = params->sample_rate > 0.0f ? params->sample_rate
                                                     : (info && info->sample_rate > 0.0f ? info->sample_rate : 48000.0f);
    float peak          = state->prev_peak;
    float attack_coeff  = expf(-1.0f / (params->attack * sample_rate + 1.0f));
    float release_coeff = expf(-1.0f / (params->release * sample_rate + 1.0f));

    const uint32_t frames = apg_process_frames_or_default(info);
    for (uint32_t i = 0; i < frames; ++i) {
        float abs_x = fabsf(in->signal[i]);
        if (abs_x > peak) {
            peak = abs_x + attack_coeff * (peak - abs_x);
        } else {
            peak = abs_x + release_coeff * (peak - abs_x);
        }
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
