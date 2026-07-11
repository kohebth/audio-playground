#include <apgcore/dsp/dsp_safety.h>
#include <atom/dsp_atoms.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FIR_SIZE 1024

void filter_fir_process(
    filter_fir_out_t         *out,
    filter_fir_in_t          *in,
    filter_fir_params_t      *params,
    filter_fir_state_t       *state,
    const apg_process_info_t *info
) {
    if (out->signal == NULL || in->signal == NULL || state == NULL || state->buffer == NULL)
        return;

    const uint32_t capacity = state->buffer_len > 0u ? state->buffer_len : MAX_FIR_SIZE;
    int            k_size   = params->kernel_size;
    if (k_size > (int)capacity)
        k_size = (int)capacity;
    if (k_size < 0)
        k_size = 0;

    const uint32_t frames    = apg_process_frames_or_default(info);
    uint32_t       write_pos = apg_wrap_index_i64(state->write_pos, capacity);

    for (uint32_t i = 0; i < frames; ++i) {
        state->buffer[write_pos] = in->signal[i];

        float acc = 0.0f;
        for (int k = 0; k < k_size; ++k) {
            uint32_t read_pos = apg_wrap_index_i64((int64_t)write_pos - k, capacity);
            acc += state->buffer[read_pos] * params->kernel[k];
        }

        out->signal[i] = acc;
        write_pos      = write_pos + 1u == capacity ? 0u : write_pos + 1u;
    }

    state->write_pos = (int)write_pos;
}

void filter_fir(filter_fir_out_t *out, filter_fir_in_t *in, filter_fir_params_t *params, filter_fir_state_t *state) {
    const apg_process_info_t info = apg_process_info_default();
    filter_fir_process(out, in, params, state, &info);
}
