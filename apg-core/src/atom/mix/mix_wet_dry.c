#include <atom/dsp_atoms.h>

#include "../internal/primitive_kernels.h"

#include <stddef.h>

void mix_wet_dry_process(
    mix_wet_dry_out_t           *out,
    const mix_wet_dry_in_t      *in,
    const mix_wet_dry_params_t  *params,
    mix_wet_dry_state_t         *state,
    const apg_process_context_t *info
) {
    if (!apg_process_context_valid(info))
        return;
    if (out == NULL || in == NULL || params == NULL || state == NULL || out->signal == NULL || in->dry == NULL ||
        in->wet == NULL)
        return;
    apg_crossfade_kernel(out->signal, in->dry, in->wet, params->mix, 0, apg_process_context_frames(info));
}
