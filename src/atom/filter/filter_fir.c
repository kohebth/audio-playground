#include <atom/dsp_atoms.h>
#include <apgcore/dsp/dsp_safety.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FIR_SIZE 1024u

void filter_fir_process(
    filter_fir_out_t         *out,
    filter_fir_in_t          *in,
    filter_fir_params_t      *params,
    filter_fir_state_t       *state,
    const apg_process_info_t *info
) {
    if (out == NULL || in == NULL || out->signal == NULL || in->signal == NULL || params == NULL || state == NULL ||
        state->buffer == NULL)
        return;

    int k_size = params->kernel_size;
    if (k_size < 0)
        k_size = 0;
    if (k_size > (int)MAX_FIR_SIZE)
        k_size = (int)MAX_FIR_SIZE;
    if (k_size > 0 && params->kernel == NULL)
        k_size = 0;

    uint32_t write_pos = apg_wrap_index_i64(state->write_pos, MAX_FIR_SIZE);
    const uint32_t frames = apg_process_frames_or_default(info);

    for (uint32_t i = 0; i < frames; ++i) {
        const float sample = isfinite(in->signal[i]) ? in->signal[i] : 0.0f;
        state->buffer[write_pos] = sample;

        float acc = 0.0f;
        for (int k = 0; k < k_size; ++k) {
            const uint32_t read_pos = apg_wrap_index_i64((int64_t)write_pos - k, MAX_FIR_SIZE);
            const float coefficient = isfinite(params->kernel[k]) ? params->kernel[k] : 0.0f;
            acc += state->buffer[read_pos] * coefficient;
        }

        out->signal[i] = isfinite(acc) ? apg_denormal_kill(acc) : 0.0f;
        write_pos = write_pos + 1u == MAX_FIR_SIZE ? 0u : write_pos + 1u;
    }

    state->write_pos = (int)write_pos;
}

void filter_fir(filter_fir_out_t *out, filter_fir_in_t *in, filter_fir_params_t *params, filter_fir_state_t *state) {
    const apg_process_info_t info = apg_process_info_default();
    filter_fir_process(out, in, params, state, &info);
}
