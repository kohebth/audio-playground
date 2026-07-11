#include <apgcore/dsp/dsp_safety.h>
#include <atom/dsp_atoms.h>
#include <stddef.h>

#define MAX_COMB_DELAY 48000u

void filter_comb_ff_process(
    filter_comb_ff_out_t     *out,
    filter_comb_ff_in_t      *in,
    filter_comb_ff_params_t  *params,
    filter_comb_ff_state_t   *state,
    const apg_process_info_t *info
) {
    if (out == NULL || in == NULL || params == NULL || state == NULL || out->signal == NULL || in->signal == NULL ||
        state->buffer == NULL)
        return;

    const uint32_t frames    = info != NULL ? info->frames : APG_DEFAULT_FRAMES;
    uint32_t       write_pos = apg_wrap_index_i64(state->write_pos, MAX_COMB_DELAY);
    int64_t        delay     = params->delay_samples;
    if (delay < 1)
        delay = 1;
    if (delay > (int64_t)MAX_COMB_DELAY)
        delay = MAX_COMB_DELAY;
    const float coefficient = isfinite(params->coefficient) ? apg_clamp_float(params->coefficient, -4.0f, 4.0f) : 0.0f;

    for (uint32_t i = 0; i < frames; ++i) {
        const uint32_t read_pos = apg_wrap_index_i64((int64_t)write_pos - delay, MAX_COMB_DELAY);
        float          delayed  = state->buffer[read_pos];
        if (!isfinite(delayed)) {
            delayed                 = 0.0f;
            state->buffer[read_pos] = 0.0f;
        }

        const float input  = isfinite(in->signal[i]) ? in->signal[i] : 0.0f;
        float       output = input + coefficient * delayed;
        if (!isfinite(output))
            output = 0.0f;

        out->signal[i]           = output;
        state->buffer[write_pos] = input;
        write_pos                = write_pos + 1u == MAX_COMB_DELAY ? 0u : write_pos + 1u;
    }
    state->write_pos = (int)write_pos;
}

void filter_comb_ff(
    filter_comb_ff_out_t *out, filter_comb_ff_in_t *in, filter_comb_ff_params_t *params, filter_comb_ff_state_t *state
) {
    const apg_process_info_t info = apg_process_info_default();
    filter_comb_ff_process(out, in, params, state, &info);
}
