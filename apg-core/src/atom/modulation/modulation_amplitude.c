#include <atom/dsp_atoms.h>
#include <stddef.h>

void modulation_amplitude_process(
    modulation_amplitude_out_t          *out,
    const modulation_amplitude_in_t     *in,
    const modulation_amplitude_params_t *params,
    modulation_amplitude_state_t        *state,
    const apg_process_context_t         *info
) {
    if (!apg_process_context_valid(info))
        return;
    if (out == NULL || in == NULL || params == NULL || state == NULL)
        return;
    (void)state;
    if (out->signal == NULL || in->signal == NULL || in->modulator == NULL || params == NULL)
        return;

    const uint32_t frames = apg_process_context_frames(info);
    for (uint32_t i = 0; i < frames; ++i) {
        out->signal[i] = in->signal[i] * (1.0f + params->depth * in->modulator[i]);
    }
}
