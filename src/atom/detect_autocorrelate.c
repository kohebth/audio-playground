#include <atom/dsp_atoms.h>
#include <stdlib.h>

#define CHUNK_LENGTH     512
#define MAX_AUTOCORR_LAG 1024

void detect_autocorrelate(
    detect_autocorrelate_out_t    *out,
    detect_autocorrelate_in_t     *in,
    detect_autocorrelate_params_t *params,
    detect_autocorrelate_state_t  *state
) {
    if (out->correlation == NULL || in->signal == NULL || state == NULL || state->buffer == NULL)
        return;

    int max_lag = params->max_lag;
    if (max_lag > MAX_AUTOCORR_LAG)
        max_lag = MAX_AUTOCORR_LAG;

    int write_pos = state->write_pos;
    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        state->buffer[write_pos] = in->signal[i];
        write_pos                = (write_pos + 1) % MAX_AUTOCORR_LAG;
    }

    for (int k = 0; k < max_lag; ++k) {
        float r_k = 0.0f;
        for (int n = 0; n < CHUNK_LENGTH; ++n) {
            int idx1 = (write_pos - CHUNK_LENGTH + n + MAX_AUTOCORR_LAG) % MAX_AUTOCORR_LAG;
            int idx2 = (idx1 - k + MAX_AUTOCORR_LAG) % MAX_AUTOCORR_LAG;
            r_k += state->buffer[idx1] * state->buffer[idx2];
        }
        out->correlation[k] = r_k;
    }

    for (int k = max_lag; k < CHUNK_LENGTH; ++k) {
        out->correlation[k] = 0.0f;
    }

    state->write_pos = write_pos;
}
