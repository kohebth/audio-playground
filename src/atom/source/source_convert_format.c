#include <atom/dsp_atoms.h>
#include <stddef.h>

void src_convert_format_process(
    src_convert_format_out_t          *out,
    const src_convert_format_in_t     *in,
    const src_convert_format_params_t *params,
    src_convert_format_state_t        *state,
    const apg_process_info_t          *info
) {
    if (out == NULL || in == NULL || params == NULL || state == NULL)
        return;
    if (out->signal == NULL || in->signal == NULL)
        return;

    const uint32_t frames = apg_process_frames_or_default(info);

    // Signal is always float* in this library, so this is currently a copy
    for (uint32_t i = 0; i < frames; ++i) {
        out->signal[i] = in->signal[i];
    }
}
