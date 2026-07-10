#include <atom/dsp_atoms.h>
#include <apgcore/dsp/dsp_safety.h>
#include <math.h>
#include <stddef.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void generation_lfo_process(
    generation_lfo_out_t     *out,
    generation_lfo_in_t      *in,
    generation_lfo_params_t  *params,
    generation_lfo_state_t   *state,
    const apg_process_info_t *info
) {
    (void)in;
    if (out == NULL || out->signal == NULL || params == NULL || state == NULL)
        return;

    const float sample_rate = apg_sample_rate_or_default(info);
    const float frequency   = apg_clamp_float(params->frequency, 0.0f, sample_rate * 0.45f);
    float       phase       = isfinite(state->phase) ? state->phase - floorf(state->phase) : 0.0f;
    const float phase_inc   = frequency / sample_rate;

    const uint32_t frames = apg_process_frames_or_default(info);
    for (uint32_t i = 0; i < frames; ++i) {
        float p = phase + (isfinite(params->phase_offset) ? params->phase_offset : 0.0f);
        p -= floorf(p);

        switch (params->waveform) {
        case WAVEFORM_SINE:
            out->signal[i] = sinf(2.0f * (float)M_PI * p);
            break;
        case WAVEFORM_SAW:
            out->signal[i] = 2.0f * p - 1.0f;
            break;
        case WAVEFORM_SQUARE:
            out->signal[i] = p < 0.5f ? 1.0f : -1.0f;
            break;
        case WAVEFORM_TRIANGLE:
            out->signal[i] = 4.0f * fabsf(p - floorf(p + 0.5f)) - 1.0f;
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
