#include <atom/dsp_atoms.h>
#include <stddef.h>

void generation_dc_process(
    generation_dc_out_t      *out,
    generation_dc_in_t       *in,
    generation_dc_params_t   *params,
    generation_dc_state_t    *state,
    const apg_process_info_t *info
) {
    if (out == NULL || in == NULL || params == NULL || state == NULL)
        return;
    (void)in;
    (void)state;
    if (out->signal == NULL || params == NULL)
        return;

    const uint32_t frames = apg_process_frames_or_default(info);
    for (uint32_t i = 0; i < frames; ++i) {
        out->signal[i] = params->value;
    }
}

void generation_dc(
    generation_dc_out_t *out, generation_dc_in_t *in, generation_dc_params_t *params, generation_dc_state_t *state
) {
    apg_process_info_t info = apg_process_info_default();
    generation_dc_process(out, in, params, state, &info);
}
