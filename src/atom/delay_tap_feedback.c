#include <atom/dsp_atoms.h>
#include <stddef.h>

void delay_tap_feedback_process(
    delay_tap_feedback_out_t    *out,
    delay_tap_feedback_in_t     *in,
    delay_tap_feedback_params_t *params,
    delay_tap_feedback_state_t  *state,
    const apg_process_info_t    *info
) {
    (void)state;
    if (out->signal == NULL || in->buffer == NULL || params == NULL) {
        return;
    }

    const uint32_t frames = apg_process_frames_or_default(info);
    for (uint32_t i = 0; i < frames; ++i) {
        out->signal[i] = in->buffer[in->tap_position] * params->coefficient;
    }
}

void delay_tap_feedback(
    delay_tap_feedback_out_t    *out,
    delay_tap_feedback_in_t     *in,
    delay_tap_feedback_params_t *params,
    delay_tap_feedback_state_t  *state
) {
    apg_process_info_t info = apg_process_info_default();
    delay_tap_feedback_process(out, in, params, state, &info);
}
