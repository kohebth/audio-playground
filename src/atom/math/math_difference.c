#include <atom/dsp_atoms.h>

#include "../internal/primitive_kernels.h"

#include <stddef.h>

void math_difference_process(
    math_difference_out_t          *out,
    const math_difference_in_t     *in,
    const math_difference_params_t *params,
    math_difference_state_t        *state,
    const apg_process_context_t    *info
) {
    if (!apg_process_context_valid(info))
        return;
    if (!out || !in || !params || !state || !out->signal || !in->signal)
        return;
    apg_difference_kernel(out->signal, in->signal, &state->prev_sample, apg_process_context_frames(info));
}
