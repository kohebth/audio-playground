#include <atom/dsp_atoms.h>
#include <stddef.h>

void src_downsample_process(
    src_downsample_out_t     *out,
    src_downsample_in_t      *in,
    src_downsample_params_t  *params,
    src_downsample_state_t   *state,
    const apg_process_info_t *info
) {
    if (out->signal == NULL || in->signal == NULL)
        return;

    int factor = params->factor;
    if (factor < 1)
        factor = 1;

    const uint32_t frames     = apg_process_frames_or_default(info);
    const uint32_t out_frames = frames / (uint32_t)factor;
    for (uint32_t i = 0; i < out_frames; ++i) {
        out->signal[i] = in->signal[i * (uint32_t)factor];
    }
}

void src_downsample(
    src_downsample_out_t *out, src_downsample_in_t *in, src_downsample_params_t *params, src_downsample_state_t *state
) {
    const apg_process_info_t info = apg_process_info_default();
    src_downsample_process(out, in, params, state, &info);
}
