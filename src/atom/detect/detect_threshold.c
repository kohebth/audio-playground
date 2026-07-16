#include <atom/dsp_atoms.h>
#include <stddef.h>

void detect_threshold_process(
    detect_threshold_out_t          *out,
    const detect_threshold_in_t     *in,
    const detect_threshold_params_t *params,
    detect_threshold_state_t        *state,
    const apg_process_info_t        *info
) {
    if (out == NULL || in == NULL || params == NULL || state == NULL)
        return;
    (void)state;
    if (out->gate == NULL || in->signal == NULL || params == NULL)
        return;

    const uint32_t frames = apg_process_frames_or_default(info);
    for (uint32_t i = 0; i < frames; ++i) {
        out->gate[i] = (in->signal[i] >= params->threshold) ? 1.0f : 0.0f;
    }
}
