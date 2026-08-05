#include <atom/dsp_atoms.h>

#include "../internal/primitive_kernels.h"

#include <stddef.h>

void math_integrate_process(
    math_integrate_out_t          *out,
    const math_integrate_in_t     *in,
    const math_integrate_params_t *params,
    math_integrate_state_t        *state,
    const apg_process_context_t   *info
) {
    if (!apg_process_context_valid(info))
        return;
    if (!out || !in || !params || !state || !out->signal || !in->signal)
        return;
    apg_integrate_kernel(
        out->signal, in->signal, params->leakage, &state->accumulator, apg_process_context_frames(info)
    );
}
