#include <atom/dsp_atoms.h>
#include <stdlib.h>

#define MAX_AUTOCORR_LAG 1024

void detect_autocorrelate_process(
    detect_autocorrelate_out_t    *out,
    detect_autocorrelate_in_t     *in,
    detect_autocorrelate_params_t *params,
    detect_autocorrelate_state_t  *state,
    const apg_process_info_t      *info
) {
    if (out->correlation == NULL || in->signal == NULL || state == NULL || state->buffer == NULL)
        return;

    const uint32_t frames  = apg_process_frames_or_default(info);
    int            max_lag = params->max_lag;
    if (max_lag > MAX_AUTOCORR_LAG)
        max_lag = MAX_AUTOCORR_LAG;
    if (max_lag < 0)
        max_lag = 0;
    if ((uint32_t)max_lag > frames)
        max_lag = (int)frames;

    int write_pos = state->write_pos;
    for (uint32_t i = 0; i < frames; ++i) {
        state->buffer[write_pos] = in->signal[i];
        write_pos                = (write_pos + 1) % MAX_AUTOCORR_LAG;
    }

    for (int k = 0; k < max_lag; ++k) {
        float r_k = 0.0f;
        for (uint32_t n = 0; n < frames; ++n) {
            int idx1 = (write_pos - (int)frames + (int)n + MAX_AUTOCORR_LAG) % MAX_AUTOCORR_LAG;
            int idx2 = (idx1 - k + MAX_AUTOCORR_LAG) % MAX_AUTOCORR_LAG;
            r_k += state->buffer[idx1] * state->buffer[idx2];
        }
        out->correlation[k] = r_k;
    }

    for (uint32_t k = (uint32_t)max_lag; k < frames; ++k) {
        out->correlation[k] = 0.0f;
    }

    state->write_pos = write_pos;
}

void detect_autocorrelate(
    detect_autocorrelate_out_t    *out,
    detect_autocorrelate_in_t     *in,
    detect_autocorrelate_params_t *params,
    detect_autocorrelate_state_t  *state
) {
    const apg_process_info_t info = apg_process_info_default();
    detect_autocorrelate_process(out, in, params, state, &info);
}
