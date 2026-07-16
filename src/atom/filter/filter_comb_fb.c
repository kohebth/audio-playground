#include <apgcore/dsp/dsp_safety.h>
#include <atom/dsp_atoms.h>
#include <stddef.h>

#define MAX_COMB_DELAY 48000u

void filter_comb_fb_process(
    filter_comb_fb_out_t          *out,
    const filter_comb_fb_in_t     *in,
    const filter_comb_fb_params_t *params,
    filter_comb_fb_state_t        *state,
    const apg_process_context_t   *info
) {
    if (!apg_process_context_valid(info))
        return;
    if (out == NULL || in == NULL || params == NULL || state == NULL)
        return;
    if (out == NULL || in == NULL || params == NULL || state == NULL || out->signal == NULL || in->signal == NULL ||
        state->buffer == NULL)
        return;

    const uint32_t capacity  = state->buffer_len > 0u ? state->buffer_len : MAX_COMB_DELAY;
    const uint32_t frames    = apg_process_context_frames(info);
    uint32_t       write_pos = apg_wrap_index_i64(state->write_pos, capacity);
    const float    coefficient =
        isfinite(params->coefficient) ? apg_clamp_float(params->coefficient, -0.999f, 0.999f) : 0.0f;

    for (uint32_t i = 0; i < frames; ++i) {
        float delay_val         = (in->delay != NULL) ? in->delay[i] : (float)params->delay_samples;
        delay_val               = apg_clamp_float(delay_val, 1.0f, (float)capacity);
        const int64_t  delay    = (int64_t)delay_val;
        const uint32_t read_pos = apg_wrap_index_i64((int64_t)write_pos - delay, capacity);
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
        state->buffer[write_pos] = apg_denormal_kill(output);
        write_pos                = write_pos + 1u == capacity ? 0u : write_pos + 1u;
    }
    state->write_pos = (int)write_pos;
}
