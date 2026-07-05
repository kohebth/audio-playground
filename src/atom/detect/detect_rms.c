#include <atom/dsp_atoms.h>
#include <math.h>
#include <stdlib.h>

#define MAX_RMS_WINDOW 4096

void detect_rms_process(
    detect_rms_out_t         *out,
    detect_rms_in_t          *in,
    detect_rms_params_t      *params,
    detect_rms_state_t       *state,
    const apg_process_info_t *info
) {
    if (out->level == NULL || in->signal == NULL || state == NULL || state->buffer == NULL)
        return;

    int w_size = params->window_size;
    if (w_size > MAX_RMS_WINDOW)
        w_size = MAX_RMS_WINDOW;
    if (w_size < 1)
        w_size = 1;

    const uint32_t frames    = apg_process_frames_or_default(info);
    int            write_pos = state->write_pos;
    float          sum_sq    = state->sum;

    for (uint32_t i = 0; i < frames; ++i) {
        float x    = in->signal[i];
        float x_sq = x * x;

        sum_sq -= state->buffer[write_pos];
        state->buffer[write_pos] = x_sq;
        sum_sq += x_sq;

        write_pos     = (write_pos + 1) % w_size;
        out->level[i] = sqrtf(fmaxf(0.0f, sum_sq / (float)w_size));
    }

    state->write_pos = write_pos;
    state->sum       = sum_sq;
}

void detect_rms(detect_rms_out_t *out, detect_rms_in_t *in, detect_rms_params_t *params, detect_rms_state_t *state) {
    const apg_process_info_t info = apg_process_info_default();
    detect_rms_process(out, in, params, state, &info);
}
