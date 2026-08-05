#include <apgcore/dsp/dsp_safety.h>
#include <atom/dsp_atoms.h>
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

apg_stream_result_t src_downsample_process(
    src_downsample_out_t          *out,
    const src_downsample_in_t     *in,
    const src_downsample_params_t *params,
    src_downsample_state_t        *state,
    const apg_stream_context_t    *context
) {
    apg_stream_result_t result = apg_stream_result_empty();
    if (!apg_stream_context_valid(context) || out == NULL || in == NULL || params == NULL || state == NULL ||
        out->signal == NULL || in->signal == NULL)
        return result;

    const uint32_t factor = (uint32_t)clamp_downsample_factor(params);
    uint32_t       phase  = state->phase % factor;

    while (result.consumed_frames < context->input_frames) {
        if (phase == 0u) {
            if (result.produced_frames >= context->output_capacity)
                break;
            const float sample                    = in->signal[result.consumed_frames];
            out->signal[result.produced_frames++] = isfinite(sample) ? sample : 0.0f;
        }
        ++result.consumed_frames;
        phase = (phase + 1u) % factor;
    }

    state->phase = phase;
    return result;
}
