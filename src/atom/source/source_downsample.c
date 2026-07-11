#include <apgcore/dsp/dsp_safety.h>
#include <atom/dsp_atoms.h>
#include <limits.h>
#include <stddef.h>

#define APG_MAX_SRC_FACTOR 16

static int clamp_downsample_factor(const src_downsample_params_t *params) {
    int factor = params != NULL ? params->factor : 1;
    if (factor < 1)
        factor = 1;
    if (factor > APG_MAX_SRC_FACTOR)
        factor = APG_MAX_SRC_FACTOR;
    return factor;
}

void src_downsample_process(
    src_downsample_out_t     *out,
    src_downsample_in_t      *in,
    src_downsample_params_t  *params,
    src_downsample_state_t   *state,
    const apg_process_info_t *info
) {
    (void)state;

    if (out == NULL || in == NULL || out->signal == NULL || in->signal == NULL)
        return;

    const uint32_t frames          = info != NULL ? info->frames : APG_DEFAULT_FRAMES;
    const uint32_t output_capacity = apg_process_output_frames_or_default(info);
    const uint32_t factor          = (uint32_t)clamp_downsample_factor(params);
    uint32_t       out_index       = 0u;

    for (uint32_t input_index = 0u; input_index < frames && out_index < output_capacity;) {
        const float sample       = in->signal[input_index];
        out->signal[out_index++] = isfinite(sample) ? sample : 0.0f;

        if (input_index > UINT32_MAX - factor)
            break;
        input_index += factor;
    }
}

void src_downsample(
    src_downsample_out_t *out, src_downsample_in_t *in, src_downsample_params_t *params, src_downsample_state_t *state
) {
    const apg_process_info_t info = apg_process_info_default();
    src_downsample_process(out, in, params, state, &info);
}
