#include <atom/dsp_atoms.h>
#include <stddef.h>
#include <string.h>

void mix_matrix_process(
    mix_matrix_out_t         *out,
    mix_matrix_in_t          *in,
    mix_matrix_params_t      *params,
    mix_matrix_state_t       *state,
    const apg_process_info_t *info
) {
    if (out == NULL || in == NULL || params == NULL || state == NULL)
        return;
    (void)state;

    if (out == NULL || in == NULL || params == NULL)
        return;
    if (out->signals == NULL || in->signals == NULL || params->coefficients == NULL)
        return;
    if (params->num_in <= 0 || params->num_out <= 0)
        return;

    const uint32_t frames = apg_process_frames_or_default(info);

    for (int j = 0; j < params->num_out; j++) {
        if (out->signals[j] == NULL)
            continue;

        memset(out->signals[j], 0, frames * sizeof(float));
        for (int i = 0; i < params->num_in; i++) {
            if (in->signals[i] == NULL || params->coefficients[j] == NULL)
                continue;

            const float g = params->coefficients[j][i];
            for (uint32_t k = 0; k < frames; k++)
                out->signals[j][k] += in->signals[i][k] * g;
        }
    }
}

void mix_matrix(mix_matrix_out_t *out, mix_matrix_in_t *in, mix_matrix_params_t *params, mix_matrix_state_t *state) {
    const apg_process_info_t info = apg_process_info_default();
    mix_matrix_process(out, in, params, state, &info);
}
