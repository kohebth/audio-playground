#include <apgcore/dsp/dsp_safety.h>
#include <atom/dsp_atoms.h>
#include <float.h>
#include <math.h>
#include <stddef.h>

void detect_autocorrelate_process(
    detect_autocorrelate_out_t          *out,
    const detect_autocorrelate_in_t     *in,
    const detect_autocorrelate_params_t *params,
    detect_autocorrelate_state_t        *state,
    const apg_process_info_t            *info
) {
    if (out == NULL || in == NULL || params == NULL || state == NULL)
        return;
    if (out == NULL || in == NULL || params == NULL || state == NULL || out->correlation == NULL ||
        in->signal == NULL || state->buffer == NULL)
        return;

    const uint32_t capacity        = state->buffer_len > 0u ? state->buffer_len : APG_DETECT_AUTOCORRELATION_CAPACITY;
    const uint32_t frames          = apg_process_frames_or_default(info);
    const uint32_t analysis_frames = frames < capacity ? frames : capacity;
    int            max_lag         = params->max_lag;
    if (max_lag > (int)capacity)
        max_lag = (int)capacity;
    if (max_lag < 0)
        max_lag = 0;
    if ((uint32_t)max_lag > analysis_frames)
        max_lag = (int)analysis_frames;

    uint32_t write_pos = apg_wrap_index_i64(state->write_pos, capacity);
    for (uint32_t i = 0; i < frames; ++i) {
        state->buffer[write_pos] = isfinite(in->signal[i]) ? in->signal[i] : 0.0f;
        write_pos                = write_pos + 1u == capacity ? 0u : write_pos + 1u;
    }

    for (int k = 0; k < max_lag; ++k) {
        double r_k = 0.0;
        for (uint32_t n = 0; n < analysis_frames; ++n) {
            const uint32_t idx1 =
                apg_wrap_index_i64((int64_t)write_pos - (int64_t)analysis_frames + (int64_t)n, capacity);
            const uint32_t idx2    = apg_wrap_index_i64((int64_t)idx1 - (int64_t)k, capacity);
            float          sample1 = state->buffer[idx1];
            float          sample2 = state->buffer[idx2];
            if (!isfinite(sample1)) {
                sample1             = 0.0f;
                state->buffer[idx1] = 0.0f;
            }
            if (!isfinite(sample2)) {
                sample2             = 0.0f;
                state->buffer[idx2] = 0.0f;
            }
            r_k += (double)sample1 * (double)sample2;
        }
        out->correlation[k] = r_k > (double)FLT_MAX ? FLT_MAX : (r_k < -(double)FLT_MAX ? -FLT_MAX : (float)r_k);
    }

    for (uint32_t k = (uint32_t)max_lag; k < frames; ++k) {
        out->correlation[k] = 0.0f;
    }

    state->write_pos = (int)write_pos;
}
