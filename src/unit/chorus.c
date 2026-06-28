#include <unit/chorus.h>

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

static filter_biquad_params_t calculate_high_shelf(float freq, float gain_db, float q, float sr) {
    filter_biquad_params_t coeffs;
    float                  A       = powf(10.0f, gain_db / 40.0f);
    float                  w0      = 2.0f * (float)M_PI * freq / sr;
    float                  cosw0   = cosf(w0);
    float                  alpha   = sinf(w0) / (2.0f * q);
    float                  sqrtA2  = 2.0f * sqrtf(A) * alpha;

    float b0 = A * ((A + 1.0f) + (A - 1.0f) * cosw0 + sqrtA2);
    float b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosw0);
    float b2 = A * ((A + 1.0f) + (A - 1.0f) * cosw0 - sqrtA2);
    float a0 = (A + 1.0f) - (A - 1.0f) * cosw0 + sqrtA2;
    float a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cosw0);
    float a2 = (A + 1.0f) - (A - 1.0f) * cosw0 - sqrtA2;

    coeffs.b0 = b0 / a0;
    coeffs.b1 = b1 / a0;
    coeffs.b2 = b2 / a0;
    coeffs.a1 = a1 / a0;
    coeffs.a2 = a2 / a0;
    return coeffs;
}

bool chorus_process_frames(chorus_out_t out, chorus_in_t in, ChorusParams params, ChorusState *state, uint32_t frames) {
    if (out.signal == NULL || in.signal == NULL || state == NULL || frames == 0u)
        return false;
    if (frames > SIZE_MAX / (4u * sizeof(float)))
        return false;

    float *scratch = calloc((size_t)frames * 4u, sizeof(float));
    if (!scratch)
        return false;

    float *lfo_out    = scratch;
    float *pre_out    = lfo_out + frames;
    float *mod_out    = pre_out + frames;
    float *wet_signal = mod_out + frames;

    const float sample_rate = params.sample_rate > 0u ? (float)params.sample_rate : 48000.0f;
    const apg_process_info_t info = {
        .sample_rate   = sample_rate,
        .frames        = frames,
        .output_frames = frames,
        .channels      = 1u,
    };

    generation_lfo_out_t    lfo_o = {lfo_out};
    generation_lfo_params_t lfo_p = {params.rate, WAVEFORM_SINE, 0.0f, sample_rate};
    generation_lfo_process(&lfo_o, NULL, &lfo_p, (generation_lfo_state_t *)&state->lfo_state, &info);

    filter_biquad_params_t pre_bq = calculate_high_shelf(3000.0f, 6.0f, 0.707f, sample_rate);
    filter_biquad_out_t    pre_o  = {pre_out};
    filter_biquad_in_t     pre_i  = {in.signal};
    filter_biquad_process(&pre_o, &pre_i, &pre_bq, (filter_biquad_state_t *)&state->pre_shelf_state, &info);

    modulation_phase_out_t    mp_o = {mod_out};
    modulation_phase_in_t     mp_i = {pre_out, lfo_out};
    modulation_phase_params_t mp_p = {params.depth};
    modulation_phase_process(&mp_o, &mp_i, &mp_p, (modulation_phase_state_t *)&state->mod_state, &info);

    filter_biquad_params_t de_bq = calculate_high_shelf(3000.0f, -6.0f, 0.707f, sample_rate);
    filter_biquad_out_t    de_o  = {wet_signal};
    filter_biquad_in_t     de_i  = {mod_out};
    filter_biquad_process(&de_o, &de_i, &de_bq, (filter_biquad_state_t *)&state->de_shelf_state, &info);

    mix_wet_dry_out_t    mw_o = {out.signal};
    mix_wet_dry_in_t     mw_i = {in.signal, wet_signal};
    mix_wet_dry_params_t mw_p = {0.5f};
    mix_wet_dry_process(&mw_o, &mw_i, &mw_p, NULL, &info);

    free(scratch);
    return true;
}

void chorus_process(chorus_out_t out, chorus_in_t in, ChorusParams params, ChorusState *state) {
    (void)chorus_process_frames(out, in, params, state, APG_DEFAULT_FRAMES);
}
