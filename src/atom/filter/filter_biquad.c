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

static int clamp_mode(int mode) {
    if (mode < 0)
        return 0;
    if (mode > 3)
        return 3;
    return mode;
}

static void biquad_mode_coefficients(
    int mode, float cutoff, float q, float sample_rate, float *b0, float *b1, float *b2, float *a1, float *a2
) {
    sample_rate = sample_rate > 1.0f ? sample_rate : 48000.0f;
    cutoff      = clamp_float(cutoff, 1.0f, sample_rate * 0.49f);
    q           = clamp_float(q, 0.05f, 20.0f);
    mode        = clamp_mode(mode);

    const float omega     = 2.0f * (float)M_PI * cutoff / sample_rate;
    const float cos_omega = cosf(omega);
    const float sin_omega = sinf(omega);
    const float alpha     = sin_omega / (2.0f * q);
    const float a0        = 1.0f + alpha;
    const float inv_a0    = 1.0f / a0;

    float raw_b0 = 0.0f;
    float raw_b1 = 0.0f;
    float raw_b2 = 0.0f;
    switch (mode) {
    case 1: // highpass
        raw_b0 = (1.0f + cos_omega) * 0.5f;
        raw_b1 = -(1.0f + cos_omega);
        raw_b2 = raw_b0;
        break;
    case 2: // bandpass, constant skirt gain
        raw_b0 = alpha;
        raw_b1 = 0.0f;
        raw_b2 = -alpha;
        break;
    case 3: // notch
        raw_b0 = 1.0f;
        raw_b1 = -2.0f * cos_omega;
        raw_b2 = 1.0f;
        break;
    case 0: // lowpass
    default:
        raw_b0 = (1.0f - cos_omega) * 0.5f;
        raw_b1 = 1.0f - cos_omega;
        raw_b2 = raw_b0;
        break;
    }

    const float raw_a1 = -2.0f * cos_omega;
    const float raw_a2 = 1.0f - alpha;
    *b0                = raw_b0 * inv_a0;
    *b1                = raw_b1 * inv_a0;
    *b2                = raw_b2 * inv_a0;
    *a1                = raw_a1 * inv_a0;
    *a2                = raw_a2 * inv_a0;
}

void filter_biquad_process(
    filter_biquad_out_t      *out,
    filter_biquad_in_t       *in,
    filter_biquad_params_t   *params,
    filter_biquad_state_t    *state,
    const apg_process_info_t *info
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

    float     z1             = state->z1;
    float     z2             = state->z2;
    float     current_cutoff = state->current_cutoff;
    float     current_q      = state->current_q;
    float     current_b0     = state->current_b0;
    float     current_b1     = state->current_b1;
    float     current_b2     = state->current_b2;
    float     current_a1     = state->current_a1;
    float     current_a2     = state->current_a2;
    float     target_cutoff  = clamp_float(params->cutoff, 1.0f, max_cutoff);
    float     target_q       = clamp_float(params->q, 0.05f, 20.0f);
    const int target_mode    = clamp_mode(params->mode);

    if (current_cutoff <= 0.0f) {
        current_cutoff = target_cutoff;
        current_q      = target_q;
        biquad_mode_coefficients(
            target_mode, current_cutoff, current_q, sample_rate, &current_b0, &current_b1, &current_b2, &current_a1,
            &current_a2
        );
    } else if (current_q <= 0.0f) {
        current_q = target_q;
    }

    for (uint32_t i = 0; i < frames; ++i) {
        target_cutoff   = clamp_float(in->cutoff ? in->cutoff[i] : params->cutoff, 1.0f, max_cutoff);
        target_q        = clamp_float(params->q, 0.05f, 20.0f);
        float target_b0 = 0.0f;
        float target_b1 = 0.0f;
        float target_b2 = 0.0f;
        float target_a1 = 0.0f;
        float target_a2 = 0.0f;
        biquad_mode_coefficients(
            target_mode, target_cutoff, target_q, sample_rate, &target_b0, &target_b1, &target_b2, &target_a1,
            &target_a2
        );

        current_cutoff = smooth > 0.0f ? target_cutoff + smooth * (current_cutoff - target_cutoff) : target_cutoff;
        current_q      = smooth > 0.0f ? target_q + smooth * (current_q - target_q) : target_q;
        current_b0     = smooth > 0.0f ? target_b0 + smooth * (current_b0 - target_b0) : target_b0;
        current_b1     = smooth > 0.0f ? target_b1 + smooth * (current_b1 - target_b1) : target_b1;
        current_b2     = smooth > 0.0f ? target_b2 + smooth * (current_b2 - target_b2) : target_b2;
        current_a1     = smooth > 0.0f ? target_a1 + smooth * (current_a1 - target_a1) : target_a1;
        current_a2     = smooth > 0.0f ? target_a2 + smooth * (current_a2 - target_a2) : target_a2;

        float x0       = in->signal[i];
        float y0       = current_b0 * x0 + z1;
        z1             = current_b1 * x0 - current_a1 * y0 + z2;
        z2             = current_b2 * x0 - current_a2 * y0;
        out->signal[i] = y0;
    }

    state->z1             = z1;
    state->z2             = z2;
    state->current_cutoff = current_cutoff;
    state->current_q      = current_q;
    state->current_b0     = current_b0;
    state->current_b1     = current_b1;
    state->current_b2     = current_b2;
    state->current_a1     = current_a1;
    state->current_a2     = current_a2;
}

void filter_biquad(
    filter_biquad_out_t *out, filter_biquad_in_t *in, filter_biquad_params_t *params, filter_biquad_state_t *state
) {
    apg_process_info_t info = apg_process_info_default();
    filter_biquad_process(out, in, params, state, &info);
}
