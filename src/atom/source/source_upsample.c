#include <apgcore/dsp/dsp_safety.h>
#include <atom/dsp_atoms.h>
#include <stddef.h>

#define APG_MAX_SRC_FACTOR 16

static int clamp_upsample_factor(const src_upsample_params_t *params) {
    int factor = params != NULL ? params->factor : 1;
    if (factor < 1)
        factor = 1;
    if (factor > APG_MAX_SRC_FACTOR)
        factor = APG_MAX_SRC_FACTOR;
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

    const int      factor = clamp_upsample_factor(params);
    const uint32_t frames = info != NULL ? info->frames : APG_DEFAULT_FRAMES;
    const uint32_t output_frames =
        info != NULL && info->output_frames > 0u ? info->output_frames : (factor == 1 ? frames : 0u);
    uint32_t out_index = 0u;

    for (uint32_t i = 0; i < frames && out_index < output_frames; ++i) {
        const float sample       = isfinite(in->signal[i]) ? in->signal[i] : 0.0f;
        out->signal[out_index++] = sample;

        for (int phase = 1; phase < factor && out_index < output_frames; ++phase) {
            out->signal[out_index++] = 0.0f;
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
