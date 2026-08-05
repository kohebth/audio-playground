#include <atom/dsp_atoms.h>

#include "../internal/primitive_kernels.h"

#include <stddef.h>

void filter_integrate_process(
    filter_integrate_out_t          *out,
    const filter_integrate_in_t     *in,
    const filter_integrate_params_t *params,
    filter_integrate_state_t        *state,
    const apg_process_context_t     *info
) {
    if (!apg_process_context_valid(info))
        return;
    if (out == NULL || in == NULL || params == NULL || state == NULL || out->signal == NULL || in->signal == NULL)
        return;
    apg_integrate_kernel(out->signal, in->signal, 0.999f, &state->accumulator, apg_process_context_frames(info));
}
