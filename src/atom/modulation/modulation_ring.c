#include <atom/dsp_atoms.h>
#include <stddef.h>

void modulation_ring_process(
    modulation_ring_out_t          *out,
    const modulation_ring_in_t     *in,
    const modulation_ring_params_t *params,
    modulation_ring_state_t        *state,
    const apg_process_info_t       *info
) {
    if (out == NULL || in == NULL || params == NULL || state == NULL)
        return;
    if (out->signal == NULL || in->signal == NULL || in->modulator == NULL)
        return;

    const uint32_t frames = apg_process_frames_or_default(info);

    for (uint32_t i = 0; i < frames; ++i) {
        out->signal[i] = in->signal[i] * in->modulator[i];
    }
}
