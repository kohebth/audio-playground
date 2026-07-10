#ifndef AUDIO_PLAYGROUND_APGCORE_DSP_SAFETY_H
#define AUDIO_PLAYGROUND_APGCORE_DSP_SAFETY_H

#include <math.h>
#include <stdint.h>

#include <apgcore/runtime/process.h>

static inline float apg_sample_rate_or_default(const apg_process_info_t *info) {
    return info && isfinite(info->sample_rate) && info->sample_rate > 1.0f ? info->sample_rate : 48000.0f;
}

static inline float apg_clamp_float(float value, float min_value, float max_value) {
    if (!isfinite(value))
        return min_value;
    if (value < min_value)
        return min_value;
    if (value > max_value)
        return max_value;
    return value;
}

static inline float apg_denormal_kill(float value) {
    return fabsf(value) < 1.0e-20f ? 0.0f : value;
}

static inline int apg_biquad_coefficients_are_finite(float b0, float b1, float b2, float a1, float a2) {
    return isfinite(b0) && isfinite(b1) && isfinite(b2) && isfinite(a1) && isfinite(a2);
}

static inline int apg_biquad_denominator_is_stable(float a1, float a2) {
    if (!isfinite(a1) || !isfinite(a2))
        return 0;
    return fabsf(a2) < 1.0f && (1.0f + a1 + a2) > 0.0f && (1.0f - a1 + a2) > 0.0f;
}

static inline uint32_t apg_wrap_index_i64(int64_t index, uint32_t length) {
    if (length == 0u)
        return 0u;
    int64_t wrapped = index % (int64_t)length;
    if (wrapped < 0)
        wrapped += (int64_t)length;
    return (uint32_t)wrapped;
}

#endif // AUDIO_PLAYGROUND_APGCORE_DSP_SAFETY_H
