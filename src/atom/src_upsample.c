#include <atom/dsp_atoms.h>
#include <stddef.h>

#define MAX_UPSAMPLE_FACTOR 8

static int clamp_upsample_factor(const src_upsample_params_t *params) {
    int factor = params != NULL ? params->factor : 1;
    if (factor < 1)
        factor = 1;
    if (factor > MAX_UPSAMPLE_FACTOR)
        factor = MAX_UPSAMPLE_FACTOR;
    return factor;
}

void src_upsample_process(
    src_upsample_out_t       *out,
    src_upsample_in_t        *in,
    src_upsample_params_t    *params,
    src_upsample_state_t     *state,
    const apg_process_info_t *info
) {
    (void)state;

    if (out == NULL || in == NULL || out->signal == NULL || in->signal == NULL)
        return;

    const int      factor        = clamp_upsample_factor(params);
    const uint32_t frames        = apg_process_frames_or_default(info);
    const uint32_t output_frames = apg_process_output_frames_or_default(info);

    for (uint32_t i = 0; i < frames; ++i) {
        const uint32_t base = i * (uint32_t)factor;
        if (base >= output_frames)
            break;

        out->signal[base] = in->signal[i];
        for (int j = 1; j < factor; ++j) {
            const uint32_t out_index = base + (uint32_t)j;
            if (out_index >= output_frames)
                break;
            out->signal[out_index] = 0.0f;
        }
    }
}

void src_upsample(
    src_upsample_out_t *out, src_upsample_in_t *in, src_upsample_params_t *params, src_upsample_state_t *state
) {
    apg_process_info_t info = apg_process_info_default();
    info.output_frames      = info.frames * (uint32_t)clamp_upsample_factor(params);
    src_upsample_process(out, in, params, state, &info);
}
