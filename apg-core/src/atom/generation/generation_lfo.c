#include <atom/dsp_atoms.h>

#include "../internal/primitive_kernels.h"

#include <stddef.h>

void generation_lfo_process(
    generation_lfo_out_t          *out,
    const generation_lfo_in_t     *in,
    const generation_lfo_params_t *params,
    generation_lfo_state_t        *state,
    const apg_process_context_t   *info
) {
    if (!apg_process_context_valid(info))
        return;
    if (out == NULL || in == NULL || params == NULL || state == NULL || out->signal == NULL)
        return;
    apg_oscillator_kernel(
        out->signal, NULL, params->frequency, params->waveform, params->phase_offset, &state->phase, info
    );
}
