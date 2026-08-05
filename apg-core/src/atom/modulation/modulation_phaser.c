#include <apgcore/dsp/dsp_safety.h>
#include <atom/dsp_atoms.h>

#include <math.h>
#include <stddef.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

enum {
    APG_PHASER_STAGES          = 6,
    APG_PHASER_HISTORY_SAMPLES = APG_PHASER_STAGES * 2,
};

void modulation_phaser_process(
    modulation_phaser_out_t          *out,
    const modulation_phaser_in_t     *in,
    const modulation_phaser_params_t *params,
    modulation_phaser_state_t        *state,
    const apg_process_context_t      *info
) {
    if (!apg_process_context_valid(info) || out == NULL || in == NULL || params == NULL || state == NULL ||
        out->signal == NULL || in->signal == NULL || in->modulator == NULL || state->buffer == NULL ||
        state->buffer_len < APG_PHASER_HISTORY_SAMPLES)
        return;

    const float sample_rate     = apg_process_context_sample_rate(info);
    const float max_center      = fminf(4000.0f, sample_rate * 0.45f);
    const float center          = isfinite(params->center_frequency)
                                      ? apg_clamp_float(params->center_frequency, 20.0f, max_center)
                                      : fminf(900.0f, max_center);
    const float depth           = isfinite(params->depth) ? apg_clamp_float(params->depth, 0.0f, 1.0f) : 0.7f;
    const float feedback        = isfinite(params->feedback) ? apg_clamp_float(params->feedback, -0.85f, 0.85f) : 0.0f;
    float       feedback_sample = isfinite(state->feedback_sample) ? apg_denormal_kill(state->feedback_sample) : 0.0f;

    const uint32_t frames = apg_process_context_frames(info);
    for (uint32_t frame = 0u; frame < frames; frame++) {
        const float modulator =
            isfinite(in->modulator[frame]) ? apg_clamp_float(in->modulator[frame], -1.0f, 1.0f) : 0.0f;
        const float frequency   = apg_clamp_float(center * exp2f(2.0f * depth * modulator), 20.0f, sample_rate * 0.45f);
        const float tangent     = tanf((float)M_PI * frequency / sample_rate);
        const float coefficient = (tangent - 1.0f) / (tangent + 1.0f);

        const float input       = isfinite(in->signal[frame]) ? in->signal[frame] : 0.0f;
        float       stage_input = input + feedback * feedback_sample;
        if (!isfinite(stage_input))
            stage_input = input;

        for (uint32_t stage = 0u; stage < APG_PHASER_STAGES; stage++) {
            const uint32_t input_history_index  = stage * 2u;
            const uint32_t output_history_index = input_history_index + 1u;
            float          input_history        = state->buffer[input_history_index];
            float          output_history       = state->buffer[output_history_index];
            if (!isfinite(input_history))
                input_history = 0.0f;
            if (!isfinite(output_history))
                output_history = 0.0f;

            float stage_output = coefficient * stage_input + input_history - coefficient * output_history;
            if (!isfinite(stage_output))
                stage_output = 0.0f;
            stage_output = apg_denormal_kill(stage_output);

            state->buffer[input_history_index]  = apg_denormal_kill(stage_input);
            state->buffer[output_history_index] = stage_output;
            stage_input                         = stage_output;
        }

        feedback_sample    = stage_input;
        out->signal[frame] = stage_input;
    }

    state->feedback_sample = feedback_sample;
}
