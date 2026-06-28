#include <atom/dsp_atoms.h>
#include <stddef.h>

void modulation_amplitude_process(
    modulation_amplitude_out_t    *out,
    modulation_amplitude_in_t     *in,
    modulation_amplitude_params_t *params,
    modulation_amplitude_state_t  *state,
    const apg_process_info_t      *info
) {
    (void)state;
    if (out->signal == NULL || in->signal == NULL || in->modulator == NULL || params == NULL)
        return;

    const uint32_t frames = apg_process_frames_or_default(info);
    for (uint32_t i = 0; i < frames; ++i) {
        out->signal[i] = in->signal[i] * (1.0f + params->depth * in->modulator[i]);
    }
}

void modulation_amplitude(
    modulation_amplitude_out_t    *out,
    modulation_amplitude_in_t     *in,
    modulation_amplitude_params_t *params,
    modulation_amplitude_state_t  *state
) {
    apg_process_info_t info = apg_process_info_default();
    modulation_amplitude_process(out, in, params, state, &info);
}
