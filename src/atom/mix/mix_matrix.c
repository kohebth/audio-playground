#include <atom/dsp_atoms.h>
#include <math.h>
#include <stddef.h>
#include <string.h>

#define APG_MIX_MATRIX_MAX_CHANNELS 8

void mix_matrix_process(
    mix_matrix_out_t         *out,
    mix_matrix_in_t          *in,
    mix_matrix_params_t      *params,
    mix_matrix_state_t       *state,
    const apg_process_info_t *info
) {
    (void)state;

    if (out == NULL || in == NULL || params == NULL)
        return;
    if (out->signals == NULL || in->signals == NULL || params->coefficients == NULL)
        return;

    int num_in  = params->num_in;
    int num_out = params->num_out;
    if (num_in <= 0 || num_out <= 0)
        return;
    if (num_in > APG_MIX_MATRIX_MAX_CHANNELS)
        num_in = APG_MIX_MATRIX_MAX_CHANNELS;
    if (num_out > APG_MIX_MATRIX_MAX_CHANNELS)
        num_out = APG_MIX_MATRIX_MAX_CHANNELS;

    const uint32_t frames = apg_process_frames_or_default(info);

    for (int j = 0; j < num_out; j++) {
        if (out->signals[j] == NULL)
            continue;

        memset(out->signals[j], 0, frames * sizeof(float));
        if (params->coefficients[j] == NULL)
            continue;

        for (int i = 0; i < num_in; i++) {
            if (in->signals[i] == NULL)
                continue;

            const float gain = isfinite(params->coefficients[j][i]) ? params->coefficients[j][i] : 0.0f;
            for (uint32_t k = 0; k < frames; k++) {
                const float sample = isfinite(in->signals[i][k]) ? in->signals[i][k] : 0.0f;
                out->signals[j][k] += sample * gain;
            }
        }
    }
}

void mix_matrix(mix_matrix_out_t *out, mix_matrix_in_t *in, mix_matrix_params_t *params, mix_matrix_state_t *state) {
    const apg_process_info_t info = apg_process_info_default();
    mix_matrix_process(out, in, params, state, &info);
}
