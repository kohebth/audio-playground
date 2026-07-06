#include <atom/dsp_atoms.h>

#include <math.h>
#include <stddef.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static float clamp_float(float value, float min_value, float max_value) {
    if (value < min_value)
        return min_value;
    if (value > max_value)
        return max_value;
    return value;
}

static void
lowpass_coefficients(float cutoff, float q, float sample_rate, float *b0, float *b1, float *b2, float *a1, float *a2) {
    sample_rate = sample_rate > 1.0f ? sample_rate : 48000.0f;
    cutoff      = clamp_float(cutoff, 1.0f, sample_rate * 0.49f);
    q           = clamp_float(q, 0.05f, 20.0f);

    const float omega     = 2.0f * (float)M_PI * cutoff / sample_rate;
    const float cos_omega = cosf(omega);
    const float sin_omega = sinf(omega);
    const float alpha     = sin_omega / (2.0f * q);
    const float a0        = 1.0f + alpha;
    const float inv_a0    = 1.0f / a0;
    const float raw_b0    = (1.0f - cos_omega) * 0.5f;
    const float raw_b1    = 1.0f - cos_omega;
    const float raw_b2    = raw_b0;
    const float raw_a1    = -2.0f * cos_omega;
    const float raw_a2    = 1.0f - alpha;
    *b0                   = raw_b0 * inv_a0;
    *b1                   = raw_b1 * inv_a0;
    *b2                   = raw_b2 * inv_a0;
    *a1                   = raw_a1 * inv_a0;
    *a2                   = raw_a2 * inv_a0;
}

void filter_biquad_lowpass_process(
    filter_biquad_lowpass_out_t    *out,
    filter_biquad_lowpass_in_t     *in,
    filter_biquad_lowpass_params_t *params,
    filter_biquad_lowpass_state_t  *state,
    const apg_process_info_t       *info
) {
    if (out == NULL || in == NULL || out->signal == NULL || in->signal == NULL || params == NULL || state == NULL)
        return;

    const uint32_t frames      = apg_process_frames_or_default(info);
    const float    sample_rate = params->sample_rate > 1.0f
                                     ? params->sample_rate
                                     : (info && info->sample_rate > 1.0f ? info->sample_rate : 48000.0f);
    const float    max_cutoff  = sample_rate * 0.49f;
    const float    smooth_ms   = params->smoothing_ms > 0.0f ? params->smoothing_ms : 0.0f;
    const float    smooth      = smooth_ms > 0.0f ? expf(-1000.0f / (smooth_ms * sample_rate)) : 0.0f;

    float z1             = state->z1;
    float z2             = state->z2;
    float current_cutoff = state->current_cutoff;
    float current_q      = state->current_q;
    float target_cutoff  = clamp_float(params->cutoff, 1.0f, max_cutoff);
    float target_q       = clamp_float(params->q, 0.05f, 20.0f);

    if (current_cutoff <= 0.0f)
        current_cutoff = target_cutoff;
    if (current_q <= 0.0f)
        current_q = target_q;

    for (uint32_t i = 0; i < frames; ++i) {
        target_cutoff = clamp_float(in->cutoff ? in->cutoff[i] : params->cutoff, 1.0f, max_cutoff);
        target_q      = clamp_float(params->q, 0.05f, 20.0f);
        if (smooth > 0.0f) {
            current_cutoff = target_cutoff + smooth * (current_cutoff - target_cutoff);
            current_q      = target_q + smooth * (current_q - target_q);
        } else {
            current_cutoff = target_cutoff;
            current_q      = target_q;
        }

        float b0 = 0.0f;
        float b1 = 0.0f;
        float b2 = 0.0f;
        float a1 = 0.0f;
        float a2 = 0.0f;
        lowpass_coefficients(current_cutoff, current_q, sample_rate, &b0, &b1, &b2, &a1, &a2);

        float x0       = in->signal[i];
        float y0       = b0 * x0 + z1;
        z1             = b1 * x0 - a1 * y0 + z2;
        z2             = b2 * x0 - a2 * y0;
        out->signal[i] = y0;
    }

    state->z1             = z1;
    state->z2             = z2;
    state->current_cutoff = current_cutoff;
    state->current_q      = current_q;
}

void filter_biquad_lowpass(
    filter_biquad_lowpass_out_t    *out,
    filter_biquad_lowpass_in_t     *in,
    filter_biquad_lowpass_params_t *params,
    filter_biquad_lowpass_state_t  *state
) {
    apg_process_info_t info = apg_process_info_default();
    filter_biquad_lowpass_process(out, in, params, state, &info);
}
