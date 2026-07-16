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

apg_stream_result_t src_upsample_process(
    src_upsample_out_t          *out,
    const src_upsample_in_t     *in,
    const src_upsample_params_t *params,
    src_upsample_state_t        *state,
    const apg_stream_context_t  *context
) {
    apg_stream_result_t result = apg_stream_result_empty();
    if (!apg_stream_context_valid(context) || out == NULL || in == NULL || params == NULL || state == NULL ||
        out->signal == NULL || in->signal == NULL)
        return result;

    const uint32_t factor = (uint32_t)clamp_upsample_factor(params);
    uint32_t       phase  = state->phase < factor ? state->phase : 0u;

    while (result.produced_frames < context->output_capacity) {
        if (phase > 0u) {
            out->signal[result.produced_frames++] = 0.0f;
            --phase;
            continue;
        }
        if (result.consumed_frames >= context->input_frames)
            break;

        const float sample                    = in->signal[result.consumed_frames++];
        out->signal[result.produced_frames++] = isfinite(sample) ? sample : 0.0f;
        phase                                 = factor - 1u;
    }

    state->phase = phase;
    return result;
}
