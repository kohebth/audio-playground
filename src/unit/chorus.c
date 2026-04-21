#include <unit/chorus.h>
#include <math.h>

#define CHUNK_LENGTH 512

static filter_biquad_params_t calculate_high_shelf(float freq, float gain_db, float q, float sr) {
    filter_biquad_params_t coeffs;
    float A = powf(10.0f, gain_db / 40.0f);
    float w0 = 2.0f * (float)M_PI * freq / sr;
    float cosw0 = cosf(w0);
    float alpha = sinf(w0) / (2.0f * q);
    float sqrtA2 = 2.0f * sqrtf(A) * alpha;

    float b0 =    A*((A+1.0f) + (A-1.0f)*cosw0 + sqrtA2);
    float b1 = -2.0f*A*((A-1.0f) + (A+1.0f)*cosw0);
    float b2 =    A*((A+1.0f) + (A-1.0f)*cosw0 - sqrtA2);
    float a0 =        (A+1.0f) - (A-1.0f)*cosw0 + sqrtA2;
    float a1 =  2.0f*((A-1.0f) - (A+1.0f)*cosw0);
    float a2 =        (A+1.0f) - (A-1.0f)*cosw0 - sqrtA2;

    coeffs.b0 = b0 / a0;
    coeffs.b1 = b1 / a0;
    coeffs.b2 = b2 / a0;
    coeffs.a1 = a1 / a0;
    coeffs.a2 = a2 / a0;
    return coeffs;
}

void chorus_process(
    chorus_out_t out,
    chorus_in_t in,
    ChorusParams params,
    ChorusState *state
) {
    if (out.signal == NULL || in.signal == NULL || state == NULL) return;

    float lfo_out[CHUNK_LENGTH];
    float pre_out[CHUNK_LENGTH];
    float mod_out[CHUNK_LENGTH];
    float wet_signal[CHUNK_LENGTH];

    // 1. osc: generation_lfo
    generation_lfo(
        (generation_lfo_out_t){ lfo_out },
        NULL,
        (generation_lfo_params_t){ params.rate, WAVEFORM_SINE, 0.0f, (float)params.sample_rate },
        (generation_lfo_state_t *)&state->lfo_state
    );

    // 2. pre: filter_biquad (High Shelf Boost)
    filter_biquad_params_t pre_bq = calculate_high_shelf(3000.0f, 6.0f, 0.707f, (float)params.sample_rate);
    filter_biquad(
        (filter_biquad_out_t){ pre_out },
        (filter_biquad_in_t){ in.signal },
        (filter_biquad_params_t){ pre_bq.b0, pre_bq.b1, pre_bq.b2, pre_bq.a1, pre_bq.a2 },
        (filter_biquad_state_t *)&state->pre_shelf_state
    );

    // 3. modulate: modulation_phase
    // Note: modulation_phase needs a buffer in state. It should be initialized by caller.
    modulation_phase(
        (modulation_phase_out_t){ mod_out },
        (modulation_phase_in_t){ pre_out, lfo_out },
        (modulation_phase_params_t){ params.depth },
        (modulation_phase_state_t *)&state->mod_state
    );

    // 4. de: filter_biquad (High Shelf Cut)
    filter_biquad_params_t de_bq = calculate_high_shelf(3000.0f, -6.0f, 0.707f, (float)params.sample_rate);
    filter_biquad(
        (filter_biquad_out_t){ wet_signal },
        (filter_biquad_in_t){ mod_out },
        (filter_biquad_params_t){ de_bq.b0, de_bq.b1, de_bq.b2, de_bq.a1, de_bq.a2 },
        (filter_biquad_state_t *)&state->de_shelf_state
    );

    // 5. blend: mix_wet_dry
    mix_wet_dry(
        (mix_wet_dry_out_t){ out.signal },
        (mix_wet_dry_in_t){ in.signal, wet_signal },
        (mix_wet_dry_params_t){ 0.5f },
        NULL
    );
}
