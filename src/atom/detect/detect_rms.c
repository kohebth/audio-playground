#include <apgcore/dsp/dsp_safety.h>
#include <atom/dsp_atoms.h>
#include <math.h>
#include <stddef.h>

void detect_rms_process(
    detect_rms_out_t         *out,
    detect_rms_in_t          *in,
    detect_rms_params_t      *params,
    detect_rms_state_t       *state,
    const apg_process_info_t *info
) {
    if (out == NULL || in == NULL || params == NULL || state == NULL || out->level == NULL || in->signal == NULL ||
        state->buffer == NULL)
        return;

    int w_size = params->window_size;
    if (w_size > (int)APG_DETECT_RMS_CAPACITY)
        w_size = (int)APG_DETECT_RMS_CAPACITY;
    if (w_size < 1)
        w_size = 1;

    const uint32_t frames    = info != NULL ? info->frames : APG_DEFAULT_FRAMES;
    uint32_t       write_pos = apg_wrap_index_i64(state->write_pos, (uint32_t)w_size);
    float          sum_sq    = state->sum;
    if (!isfinite(sum_sq)) {
        sum_sq = 0.0f;
        for (int i = 0; i < w_size; ++i) {
            float buffered = state->buffer[i];
            if (!isfinite(buffered) || buffered < 0.0f) {
                buffered         = 0.0f;
                state->buffer[i] = 0.0f;
            }
            sum_sq += buffered;
            if (!isfinite(sum_sq)) {
                sum_sq = 0.0f;
                for (int reset = 0; reset < w_size; ++reset)
                    state->buffer[reset] = 0.0f;
                break;
            }
        }
    }
    if (sum_sq < 0.0f)
        sum_sq = 0.0f;

    for (uint32_t i = 0; i < frames; ++i) {
        const float x    = isfinite(in->signal[i]) ? in->signal[i] : 0.0f;
        float       x_sq = x * x;
        if (!isfinite(x_sq))
            x_sq = 0.0f;

        float previous = state->buffer[write_pos];
        if (!isfinite(previous) || previous < 0.0f) {
            previous                 = 0.0f;
            state->buffer[write_pos] = 0.0f;
        }
        sum_sq -= previous;
        if (sum_sq < 0.0f)
            sum_sq = 0.0f;
        state->buffer[write_pos] = x_sq;
        sum_sq += x_sq;
        if (!isfinite(sum_sq)) {
            sum_sq                   = 0.0f;
            state->buffer[write_pos] = 0.0f;
        }

        write_pos     = write_pos + 1u == (uint32_t)w_size ? 0u : write_pos + 1u;
        out->level[i] = sqrtf(fmaxf(0.0f, sum_sq / (float)w_size));
    }

    state->write_pos = (int)write_pos;
    state->sum       = apg_denormal_kill(sum_sq);
}

void detect_rms(detect_rms_out_t *out, detect_rms_in_t *in, detect_rms_params_t *params, detect_rms_state_t *state) {
    const apg_process_info_t info = apg_process_info_default();
    detect_rms_process(out, in, params, state, &info);
}
