#include <atom/dsp_atoms.h>
#include <stddef.h>

void generation_envelope_process(
    generation_envelope_out_t    *out,
    generation_envelope_in_t     *in,
    generation_envelope_params_t *params,
    generation_envelope_state_t  *state,
    const apg_process_info_t     *info
) {
    if (out->signal == NULL || in->gate == NULL || state == NULL)
        return;

    const uint32_t frames        = apg_process_frames_or_default(info);
    float          current_level = state->current_level;
    int            stage         = state->stage;

    float attack_step  = 1.0f / (params->attack * params->sample_rate + 1.0f);
    float decay_step   = (1.0f - params->sustain) / (params->decay * params->sample_rate + 1.0f);
    float release_step = params->sustain / (params->release * params->sample_rate + 1.0f);

    for (uint32_t i = 0; i < frames; ++i) {
        if (in->gate[i] > 0.5f) {
            if (stage == 0 || stage == 4)
                stage = 1;

            if (stage == 1) {
                current_level += attack_step;
                if (current_level >= 1.0f) {
                    current_level = 1.0f;
                    stage         = 2;
                }
            } else if (stage == 2) {
                current_level -= decay_step;
                if (current_level <= params->sustain) {
                    current_level = params->sustain;
                    stage         = 3;
                }
            } else if (stage == 3) {
                current_level = params->sustain;
            }
        } else {
            if (stage != 0)
                stage = 4;

            if (stage == 4) {
                current_level -= release_step;
                if (current_level <= 0.0f) {
                    current_level = 0.0f;
                    stage         = 0;
                }
            }
        }
        out->signal[i] = current_level;
    }

    state->current_level = current_level;
    state->stage         = stage;
}

void generation_envelope(
    generation_envelope_out_t    *out,
    generation_envelope_in_t     *in,
    generation_envelope_params_t *params,
    generation_envelope_state_t  *state
) {
    const apg_process_info_t info = apg_process_info_default();
    generation_envelope_process(out, in, params, state, &info);
}
