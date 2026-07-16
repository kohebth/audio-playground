#include <apgcore/dsp/dsp_safety.h>
#include <atom/dsp_atoms.h>
#include <math.h>
#include <stddef.h>

#define PI                3.14159265358979323846f
#define APG_SINC_MAX_TAPS 63

void interpolation_sinc_process(
    interpolation_sinc_out_t          *out,
    const interpolation_sinc_in_t     *in,
    const interpolation_sinc_params_t *params,
    interpolation_sinc_state_t        *state,
    const apg_process_info_t          *info
) {
    if (out == NULL || in == NULL || params == NULL || state == NULL)
        return;
    if (out->signal == NULL || in->buffer == NULL || in->position == NULL)
        return;

    const uint32_t frames = apg_process_frames_or_default(info);
    if (frames == 0u)
        return;

    int taps = params->num_taps;
    if (taps < 1)
        taps = 1;
    if (taps > APG_SINC_MAX_TAPS)
        taps = APG_SINC_MAX_TAPS;
    if (taps % 2 == 0)
        taps--;
    if ((uint32_t)taps > frames)
        taps = (int)(frames % 2u == 0u ? frames - 1u : frames);
    if (taps < 1)
        taps = 1;

    const int   half_taps    = taps / 2;
    const float min_position = (float)half_taps;
    const float max_position = (float)(frames - 1u - (uint32_t)half_taps);

    for (uint32_t i = 0; i < frames; ++i) {
        float position = in->position[i];
        if (!isfinite(position))
            position = min_position;
        position = apg_clamp_float(position, min_position, max_position);

        const int center_idx = (int)floorf(position);
        float     acc        = 0.0f;
        float     weight_sum = 0.0f;

        for (int k = -half_taps; k <= half_taps; ++k) {
            const int   idx = center_idx + k;
            const float x   = (float)idx - position;
            float       w   = 1.0f;
            if (taps > 1) {
                const float window_pos = (float)(k + half_taps) / (float)(taps - 1);
                w = 0.42f - 0.5f * cosf(2.0f * PI * window_pos) + 0.08f * cosf(4.0f * PI * window_pos);
            }

            const float s           = fabsf(x) < 1.0e-6f ? 1.0f : sinf(PI * x) / (PI * x);
            const float coefficient = s * w;
            const float sample      = isfinite(in->buffer[idx]) ? in->buffer[idx] : 0.0f;
            acc += sample * coefficient;
            weight_sum += coefficient;
        }

        if (fabsf(weight_sum) > 1.0e-12f)
            acc /= weight_sum;
        out->signal[i] = isfinite(acc) ? acc : 0.0f;
    }
}
