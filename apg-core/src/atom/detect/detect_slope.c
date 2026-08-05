#include <atom/dsp_atoms.h>

#include "../internal/primitive_kernels.h"

#include <stddef.h>

void detect_slope_process(
    detect_slope_out_t          *out,
    const detect_slope_in_t     *in,
    const detect_slope_params_t *params,
    detect_slope_state_t        *state,
    const apg_process_context_t *info
) {
    if (!apg_process_context_valid(info))
        return;
    if (out == NULL || in == NULL || params == NULL || state == NULL || out->slope == NULL || in->signal == NULL)
        return;
    apg_difference_kernel(out->slope, in->signal, &state->prev_sample, apg_process_context_frames(info));
}
