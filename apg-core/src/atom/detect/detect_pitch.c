#include <apgcore/dsp/dsp_safety.h>
#include <atom/dsp_atoms.h>
#include <math.h>
#include <stddef.h>

#define MIN_PITCH_LAG 32u

void detect_pitch_process(
    detect_pitch_out_t          *out,
    const detect_pitch_in_t     *in,
    const detect_pitch_params_t *params,
    detect_pitch_state_t        *state,
    const apg_process_context_t *info
) {
    if (!apg_process_context_valid(info))
        return;
    if (out == NULL || in == NULL || params == NULL || state == NULL)
        return;
    if (out == NULL || in == NULL || params == NULL || state == NULL || out->pitch == NULL || in->signal == NULL ||
        state->buffer == NULL || state->buffer_len == 0u)
        return;

    const uint32_t capacity        = state->buffer_len;
    const uint32_t frames          = apg_process_context_frames(info);
    const uint32_t analysis_frames = frames < capacity ? frames : capacity;
    int            max_lag         = params->max_lag;
    if (max_lag > (int)capacity)
        max_lag = (int)capacity;
    if (max_lag < 0)
        max_lag = 0;
    if ((uint32_t)max_lag > analysis_frames)
        max_lag = (int)analysis_frames;

    const float sample_rate = apg_process_context_sample_rate(info);

    uint32_t write_pos = apg_wrap_index_i64(state->write_pos, capacity);
    for (uint32_t i = 0; i < frames; ++i) {
        state->buffer[write_pos] = isfinite(in->signal[i]) ? in->signal[i] : 0.0f;
        write_pos                = write_pos + 1u == capacity ? 0u : write_pos + 1u;
    }
    state->write_pos = (int)write_pos;

    double r0 = 0.0;
    for (uint32_t n = 0; n < analysis_frames; ++n) {
        const uint32_t idx = apg_wrap_index_i64((int64_t)write_pos - (int64_t)analysis_frames + (int64_t)n, capacity);
        const double   sample = state->buffer[idx];
        r0 += sample * sample;
    }

    if (!isfinite(r0) || r0 < 0.005 || (uint32_t)max_lag <= MIN_PITCH_LAG) {
        for (uint32_t i = 0; i < frames; i++)
            out->pitch[i] = 0.0f;
        return;
    }

    uint32_t best_lag      = 0u;
    double   best_corr     = -2.0;
    double   best_raw_corr = -2.0;

    for (uint32_t k = MIN_PITCH_LAG; k < (uint32_t)max_lag; ++k) {
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
        const double normalized = r_k / r0;
        const double bias       = 1.0 - (0.001 * (double)k);
        const double score      = normalized * bias;

        if (isfinite(score) && score > best_corr) {
            best_corr     = score;
            best_raw_corr = normalized;
            best_lag      = k;
        }
    }

    float detected_pitch = 0.0f;
    if (best_raw_corr > 0.85 && best_lag > 0u)
        detected_pitch = sample_rate / (float)best_lag;
    if (!isfinite(detected_pitch))
        detected_pitch = 0.0f;

    for (uint32_t i = 0; i < frames; i++) {
        out->pitch[i] = detected_pitch;
    }
}
