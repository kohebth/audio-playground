#include <apgcore/dsp/dsp_safety.h>
#include <atom/dsp_atoms.h>
#include <math.h>
#include <stddef.h>

void amplitude_gain_db_process(
    amplitude_gain_db_out_t          *out,
    const amplitude_gain_db_in_t     *in,
    const amplitude_gain_db_params_t *params,
    amplitude_gain_db_state_t        *state,
    const apg_process_context_t      *info
) {
    if (!apg_process_context_valid(info))
        return;
    if (out == NULL || in == NULL || params == NULL || state == NULL)
        return;
    if (out->signal == NULL || in->signal == NULL)
        return;

    const float    gain_db = isfinite(params->gain_db) ? apg_clamp_float(params->gain_db, -120.0f, 24.0f) : 0.0f;
    const float    gain    = powf(10.0f, gain_db / 20.0f);
    const uint32_t frames  = apg_process_context_frames(info);
    for (uint32_t i = 0; i < frames; ++i) {
        const float sample = isfinite(in->signal[i]) ? in->signal[i] : 0.0f;
        const float value  = sample * gain;
        out->signal[i]     = isfinite(value) ? value : 0.0f;
    }
}
