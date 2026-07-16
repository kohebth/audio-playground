#include <atom/dsp_atoms.h>

#include "../internal/primitive_kernels.h"

#include <stddef.h>

void amplitude_accumulate_process(
    amplitude_accumulate_out_t          *out,
    const amplitude_accumulate_in_t     *in,
    const amplitude_accumulate_params_t *params,
    amplitude_accumulate_state_t        *state,
    const apg_process_context_t         *info
) {
    if (!apg_process_context_valid(info))
        return;
    if (out == NULL || in == NULL || params == NULL || state == NULL || out->signal == NULL || in->signal == NULL)
        return;
    apg_integrate_kernel(out->signal, in->signal, 1.0f, &state->accumulator, apg_process_context_frames(info));
}
