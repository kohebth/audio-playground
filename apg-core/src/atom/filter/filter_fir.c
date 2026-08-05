#include <apgcore/dsp/dsp_safety.h>
#include <atom/dsp_atoms.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FIR_SIZE 1024

void filter_fir_process(
    filter_fir_out_t            *out,
    const filter_fir_in_t       *in,
    const filter_fir_params_t   *params,
    filter_fir_state_t          *state,
    const apg_process_context_t *info
) {
    if (!apg_process_context_valid(info))
        return;
    if (out == NULL || in == NULL || params == NULL || state == NULL)
        return;
    if (out->signal == NULL || in->signal == NULL || state->buffer == NULL || state->buffer_len == 0u)
        return;

    const uint32_t capacity = state->buffer_len;
    int            k_size   = params->kernel_size;
    if (k_size > (int)capacity)
        k_size = (int)capacity;
    if (k_size > MAX_FIR_SIZE)
        k_size = MAX_FIR_SIZE;
    if (k_size < 0)
        k_size = 0;
    if (k_size > 0 && params->kernel == NULL)
        k_size = 0;

    const uint32_t frames    = apg_process_context_frames(info);
    uint32_t       write_pos = apg_wrap_index_i64(state->write_pos, capacity);

    for (uint32_t i = 0; i < frames; ++i) {
        state->buffer[write_pos] = isfinite(in->signal[i]) ? in->signal[i] : 0.0f;

        float acc = 0.0f;
        for (int k = 0; k < k_size; ++k) {
            uint32_t    read_pos    = apg_wrap_index_i64((int64_t)write_pos - k, capacity);
            const float coefficient = isfinite(params->kernel[k]) ? params->kernel[k] : 0.0f;
            acc += state->buffer[read_pos] * coefficient;
        }

        out->signal[i] = isfinite(acc) ? apg_denormal_kill(acc) : 0.0f;
        write_pos      = write_pos + 1u == capacity ? 0u : write_pos + 1u;
    }

    state->write_pos = (int)write_pos;
}
