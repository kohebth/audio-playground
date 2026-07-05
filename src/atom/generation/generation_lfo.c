#include <atom/dsp_atoms.h>
#include <math.h>
#include <stddef.h>

void generation_lfo_process(
    generation_lfo_out_t     *out,
    generation_lfo_in_t      *in,
    generation_lfo_params_t  *params,
    generation_lfo_state_t   *state,
    const apg_process_info_t *info
) {
    (void)in;
    if (out->signal == NULL || params == NULL || state == NULL)
        return;

    float sample_rate = params->sample_rate > 0.0f ? params->sample_rate
                                                   : (info && info->sample_rate > 0.0f ? info->sample_rate : 48000.0f);
    float phase       = state->phase;
    float phase_inc   = params->frequency / sample_rate;

    const uint32_t frames = apg_process_frames_or_default(info);
    for (uint32_t i = 0; i < frames; ++i) {
        float p = phase + params->phase_offset;
        p -= floorf(p);

        switch (params->waveform) {
        case WAVEFORM_SINE:
            out->signal[i] = sinf(2.0f * (float)M_PI * p);
            break;
        case WAVEFORM_SAW:
            out->signal[i] = 2.0f * (p - floorf(p + 0.5f));
            break;
        case WAVEFORM_SQUARE:
            out->signal[i] = (p < 0.5f) ? 1.0f : -1.0f;
            break;
        case WAVEFORM_TRIANGLE:
            out->signal[i] = 4.0f * fabsf(p - floorf(p + 0.75f) + 0.25f) - 1.0f;
            break;
        default:
            out->signal[i] = 0.0f;
            break;
        }

        phase += phase_inc;
        phase -= floorf(phase);
    }

    state->phase = phase;
}

void generation_lfo(
    generation_lfo_out_t *out, generation_lfo_in_t *in, generation_lfo_params_t *params, generation_lfo_state_t *state
) {
    apg_process_info_t info = apg_process_info_default();
    generation_lfo_process(out, in, params, state, &info);
}
