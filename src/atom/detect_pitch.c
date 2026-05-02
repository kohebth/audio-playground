#include <atom/dsp_atoms.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>

#define CHUNK_LENGTH     512
#define MAX_AUTOCORR_LAG 1024
#define MIN_LAG         32   // Restored to 20 (D7) but using stronger penalty to avoid noise bug

void detect_pitch(
    detect_pitch_out_t    *out,
    detect_pitch_in_t     *in,
    detect_pitch_params_t *params,
    detect_pitch_state_t  *state
) {
    if (out->pitch == NULL || in->signal == NULL || state == NULL || state->buffer == NULL)
        return;

    int max_lag = params->max_lag;
    if (max_lag > MAX_AUTOCORR_LAG)
        max_lag = MAX_AUTOCORR_LAG;
    if (max_lag <= MIN_LAG)
        max_lag = MAX_AUTOCORR_LAG;

    float sample_rate = params->sample_rate;
    if (sample_rate <= 0.0f)
        sample_rate = 48000.0f;

    int write_pos = state->write_pos;
    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        state->buffer[write_pos] = in->signal[i];
        write_pos = (write_pos + 1) % MAX_AUTOCORR_LAG;
    }
    state->write_pos = write_pos;

    // Calculate energy (R0)
    float r0 = 0.0f;
    for (int n = 0; n < CHUNK_LENGTH; ++n) {
        int idx = (write_pos - CHUNK_LENGTH + n + MAX_AUTOCORR_LAG) % MAX_AUTOCORR_LAG;
        float s = state->buffer[idx];
        r0 += s * s;
    }

    // Ignore background noise: realistic energy threshold
    if (r0 < 0.005f) {
        for (int i = 0; i < CHUNK_LENGTH; i++) out->pitch[i] = 0.0f;
        return;
    }

    float best_lag = 0;
    float best_corr = -2.0f;
    float best_raw_corr = -2.0f;

    // Autocorrelation search
    for (int k = MIN_LAG; k < max_lag; ++k) {
        float r_k = 0.0f;
        for (int n = 0; n < CHUNK_LENGTH; ++n) {
            int idx1 = (write_pos - CHUNK_LENGTH + n + MAX_AUTOCORR_LAG) % MAX_AUTOCORR_LAG;
            int idx2 = (idx1 - k + MAX_AUTOCORR_LAG) % MAX_AUTOCORR_LAG;
            r_k += state->buffer[idx1] * state->buffer[idx2];
        }
        float normalized = r_k / r0;

        // Apply a penalty to longer lags to avoid octave drops (subharmonics)
        // Doubled penalty (0.001) to favor the fundamental more strongly
        float bias = 1.0f - (0.001f * k);
        float score = normalized * bias;

        if (score > best_corr) {
            best_corr = score;
            best_raw_corr = normalized;
            best_lag = (float)k;
        }
    }

    float detected_pitch = 0.0f;
    // High confidence threshold (0.85) ensures we only latch stable, accurate vowels
    if (best_raw_corr > 0.85f && best_lag > 0.0f) {
        detected_pitch = sample_rate / best_lag;
    }

    for (int i = 0; i < CHUNK_LENGTH; i++) {
        out->pitch[i] = detected_pitch;
    }
}