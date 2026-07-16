#include <apgcore/dsp/dsp_safety.h>
#include <atom/dsp_atoms.h>
#include <math.h>
#include <stddef.h>

#define MAX_DELAY_SAMPLES 192000u

void delay_tap_feedback_process(
    delay_tap_feedback_out_t          *out,
    const delay_tap_feedback_in_t     *in,
    const delay_tap_feedback_params_t *params,
    delay_tap_feedback_state_t        *state,
    const apg_process_info_t          *info
) {
    if (out == NULL || in == NULL || params == NULL || state == NULL)
        return;
    (void)state;
    if (out == NULL || in == NULL || out->signal == NULL || in->buffer == NULL || params == NULL)
        return;

    int64_t tap = in->tap_position;
    if (tap < 0)
        tap = 0;
    if (tap >= (int64_t)MAX_DELAY_SAMPLES)
        tap = (int64_t)MAX_DELAY_SAMPLES - 1;
    const float coefficient = isfinite(params->coefficient) ? params->coefficient : 0.0f;
    const float sample      = isfinite(in->buffer[tap]) ? in->buffer[tap] : 0.0f;

    const uint32_t frames = apg_process_frames_or_default(info);
    for (uint32_t i = 0; i < frames; ++i)
        out->signal[i] = sample * coefficient;
}
