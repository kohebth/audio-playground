#include <atom/dsp_atoms.h>

#include "../internal/primitive_kernels.h"

#include <stddef.h>

void filter_differentiate_process(
    filter_differentiate_out_t          *out,
    const filter_differentiate_in_t     *in,
    const filter_differentiate_params_t *params,
    filter_differentiate_state_t        *state,
    const apg_process_context_t         *info
) {
    if (!apg_process_context_valid(info))
        return;
    if (out == NULL || in == NULL || params == NULL || state == NULL || out->signal == NULL || in->signal == NULL)
        return;
    apg_difference_kernel(out->signal, in->signal, &state->prev_sample, apg_process_context_frames(info));
}
