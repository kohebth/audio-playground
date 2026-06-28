#include <atom/dsp_atoms.h>
#include <math.h>
#include <stddef.h>

void amplitude_smooth_process(
    amplitude_smooth_out_t    *out,
    amplitude_smooth_in_t     *in,
    amplitude_smooth_params_t *params,
    amplitude_smooth_state_t  *state,
    const apg_process_info_t  *info
) {
    if (out->signal == NULL || in->signal == NULL || params == NULL || state == NULL)
        return;

    float sample_rate = params->sample_rate > 0.0f ? params->sample_rate
                                                   : (info && info->sample_rate > 0.0f ? info->sample_rate : 48000.0f);
    float alpha_att   = 1.0f - expf(-1.0f / (params->attack * sample_rate + 1.0f));
    float alpha_rel   = 1.0f - expf(-1.0f / (params->release * sample_rate + 1.0f));
    float last_out    = state->prev_value;

    const uint32_t frames = apg_process_frames_or_default(info);
    for (uint32_t i = 0; i < frames; ++i) {
        float input = in->signal[i];
        float alpha = (input > last_out) ? alpha_att : alpha_rel;
        last_out += alpha * (input - last_out);
        out->signal[i] = last_out;
    }

    state->prev_value = last_out;
}

void amplitude_smooth(
    amplitude_smooth_out_t    *out,
    amplitude_smooth_in_t     *in,
    amplitude_smooth_params_t *params,
    amplitude_smooth_state_t  *state
) {
    apg_process_info_t info = apg_process_info_default();
    amplitude_smooth_process(out, in, params, state, &info);
}
