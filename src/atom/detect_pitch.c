#include <atom/dsp_atoms.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>

#define MAX_AUTOCORR_LAG 1024
#define MIN_LAG          32

void detect_pitch_process(
    detect_pitch_out_t       *out,
    detect_pitch_in_t        *in,
    detect_pitch_params_t    *params,
    detect_pitch_state_t     *state,
    const apg_process_info_t *info
) {
    if (out->pitch == NULL || in->signal == NULL || state == NULL || state->buffer == NULL)
        return;

    const uint32_t frames  = apg_process_frames_or_default(info);
    int            max_lag = params->max_lag;
    if (max_lag > MAX_AUTOCORR_LAG)
        max_lag = MAX_AUTOCORR_LAG;
    if (max_lag <= MIN_LAG)
        max_lag = MAX_AUTOCORR_LAG;

    float sample_rate = params->sample_rate;
    if (sample_rate <= 0.0f)
        sample_rate = 48000.0f;

    int write_pos = state->write_pos;
    for (uint32_t i = 0; i < frames; ++i) {
        state->buffer[write_pos] = in->signal[i];
        write_pos                = (write_pos + 1) % MAX_AUTOCORR_LAG;
    }
    state->write_pos = write_pos;

    float r0 = 0.0f;
    for (uint32_t n = 0; n < frames; ++n) {
        int   idx = (write_pos - (int)frames + (int)n + MAX_AUTOCORR_LAG) % MAX_AUTOCORR_LAG;
        float s   = state->buffer[idx];
        r0 += s * s;
    }

    if (r0 < 0.005f) {
        for (uint32_t i = 0; i < frames; i++)
            out->pitch[i] = 0.0f;
        return;
    }

    float best_lag      = 0;
    float best_corr     = -2.0f;
    float best_raw_corr = -2.0f;

    for (int k = MIN_LAG; k < max_lag; ++k) {
        float r_k = 0.0f;
        for (uint32_t n = 0; n < frames; ++n) {
            int idx1 = (write_pos - (int)frames + (int)n + MAX_AUTOCORR_LAG) % MAX_AUTOCORR_LAG;
            int idx2 = (idx1 - k + MAX_AUTOCORR_LAG) % MAX_AUTOCORR_LAG;
            r_k += state->buffer[idx1] * state->buffer[idx2];
        }
        float normalized = r_k / r0;
        float bias       = 1.0f - (0.001f * k);
        float score      = normalized * bias;

        if (score > best_corr) {
            best_corr     = score;
            best_raw_corr = normalized;
            best_lag      = (float)k;
        }
    }

    float detected_pitch = 0.0f;
    if (best_raw_corr > 0.85f && best_lag > 0.0f) {
        detected_pitch = sample_rate / best_lag;
    }

    for (uint32_t i = 0; i < frames; i++) {
        out->pitch[i] = detected_pitch;
    }
}

void detect_pitch(
    detect_pitch_out_t *out, detect_pitch_in_t *in, detect_pitch_params_t *params, detect_pitch_state_t *state
) {
    const apg_process_info_t info = apg_process_info_default();
    detect_pitch_process(out, in, params, state, &info);
}
