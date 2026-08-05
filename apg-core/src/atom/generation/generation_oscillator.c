#include <atom/dsp_atoms.h>

#include "../internal/primitive_kernels.h"

#include <stddef.h>

void generation_oscillator_process(
    generation_oscillator_out_t          *out,
    const generation_oscillator_in_t     *in,
    const generation_oscillator_params_t *params,
    generation_oscillator_state_t        *state,
    const apg_process_context_t          *info
) {
    if (!apg_process_context_valid(info))
        return;
    if (out == NULL || in == NULL || params == NULL || state == NULL || out->signal == NULL)
        return;
    apg_oscillator_kernel(
        out->signal, in->frequency, params->frequency, params->waveform, params->phase_offset, &state->phase, info
    );
}
