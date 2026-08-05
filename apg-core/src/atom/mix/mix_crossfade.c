#include <atom/dsp_atoms.h>

#include "../internal/primitive_kernels.h"

#include <stddef.h>

void mix_crossfade_process(
    mix_crossfade_out_t          *out,
    const mix_crossfade_in_t     *in,
    const mix_crossfade_params_t *params,
    mix_crossfade_state_t        *state,
    const apg_process_context_t  *info
) {
    if (!apg_process_context_valid(info))
        return;
    if (out == NULL || in == NULL || params == NULL || state == NULL || out->signal == NULL || in->signal_a == NULL ||
        in->signal_b == NULL)
        return;
    apg_crossfade_kernel(
        out->signal, in->signal_a, in->signal_b, params->t, params->curve, apg_process_context_frames(info)
    );
}
