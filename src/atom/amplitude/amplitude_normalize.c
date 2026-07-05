#include <atom/dsp_atoms.h>
#include <math.h>
#include <stddef.h>

void amplitude_normalize_process(
    amplitude_normalize_out_t    *out,
    amplitude_normalize_in_t     *in,
    amplitude_normalize_params_t *params,
    amplitude_normalize_state_t  *state,
    const apg_process_info_t     *info
) {
    if (out->signal == NULL || in->signal == NULL || state == NULL)
        return;

    const uint32_t frames = apg_process_frames_or_default(info);
    float          peak   = state->running_peak;
    if (params->mode == NORMALIZE_PEAK) {
        for (uint32_t i = 0; i < frames; ++i) {
            float abs_val = fabsf(in->signal[i]);
            if (abs_val > peak)
                peak = abs_val;
        }
    } else {
        float sum_sq = 0.0f;
        for (uint32_t i = 0; i < frames; ++i) {
            sum_sq += in->signal[i] * in->signal[i];
        }
        peak = sqrtf(sum_sq / (float)frames);
    }

    float gain = 1.0f;
    if (peak > 1e-6f) {
        gain = params->target_level / peak;
    }

    for (uint32_t i = 0; i < frames; ++i) {
        out->signal[i] = in->signal[i] * gain;
    }

    state->running_peak = peak;
}

void amplitude_normalize(
    amplitude_normalize_out_t    *out,
    amplitude_normalize_in_t     *in,
    amplitude_normalize_params_t *params,
    amplitude_normalize_state_t  *state
) {
    const apg_process_info_t info = apg_process_info_default();
    amplitude_normalize_process(out, in, params, state, &info);
}
