#include <unit/sustainer.h>
#include <math.h>
#include <string.h>

#define CHUNK_LENGTH 512

// Helper to calculate biquad coefficients (Standard RBJ Lowpass)
static filter_biquad_params_t calculate_lpf_coeffs(float freq, float q, float sr) {
    filter_biquad_params_t coeffs;
    float w0 = 2.0f * M_PI * freq / sr;
    float cosw0 = cosf(w0);
    float alpha = sinf(w0) / (2.0f * q);

    float b0 = (1.0f - cosw0) / 2.0f;
    float b1 =  1.0f - cosw0;
    float b2 = (1.0f - cosw0) / 2.0f;
    float a0 =  1.0f + alpha;
    float a1 = -2.0f * cosw0;
    float a2 =  1.0f - alpha;

    coeffs.b0 = b0 / a0;
    coeffs.b1 = b1 / a0;
    coeffs.b2 = b2 / a0;
    coeffs.a1 = a1 / a0;
    coeffs.a2 = a2 / a0;

    return coeffs;
}

void sustainer_process(
    sustainer_out_t out,
    sustainer_in_t in,
    SustainerParams params,
    SustainerState *state
) {
    if (out.signal == NULL || in.signal == NULL || state == NULL) return;

    float env[CHUNK_LENGTH];
    float gate[CHUNK_LENGTH];
    float raw_gain[CHUNK_LENGTH];
    float clamped_gain[CHUNK_LENGTH];
    float smooth_gain[CHUNK_LENGTH];
    float gated_gain[CHUNK_LENGTH];
    float sustained[CHUNK_LENGTH];
    float filtered[CHUNK_LENGTH];
    float trimmed[CHUNK_LENGTH];

    // Internal Constants
    float target_linear = powf(10.0f, -12.0f / 20.0f);
    float threshold_linear = powf(10.0f, params.threshold / 20.0f);
    float gain_linear_max = powf(10.0f, params.gain / 20.0f);

    // 1. detect: detect_envelope
    detect_envelope_out_t    de_out = { env };
    detect_envelope_in_t     de_in  = { in.signal };
    detect_envelope_params_t de_cfg = { params.attack, params.release, (float)params.sample_rate };
    detect_envelope(&de_out, &de_in, &de_cfg, (detect_envelope_state_t *)&state->detect_env_state);

    // 2. compare: detect_threshold
    detect_threshold_out_t    dt_out = { gate };
    detect_threshold_in_t     dt_in  = { env };
    detect_threshold_params_t dt_cfg = { threshold_linear };
    detect_threshold(&dt_out, &dt_in, &dt_cfg, NULL);

    // 3. calc_gain: amplitude_divide
    float target_sig[CHUNK_LENGTH];
    for (int i = 0; i < CHUNK_LENGTH; i++) target_sig[i] = target_linear;
    amplitude_divide_out_t    ad_out = { raw_gain };
    amplitude_divide_in_t     ad_in  = { target_sig, env };
    amplitude_divide_params_t ad_cfg = { 1e-6f };
    amplitude_divide(&ad_out, &ad_in, &ad_cfg, NULL);

    // 4. clamp_gain: amplitude_clip_hard
    amplitude_clip_hard_out_t    ac_out = { clamped_gain };
    amplitude_clip_hard_in_t     ac_in  = { raw_gain };
    amplitude_clip_hard_params_t ac_cfg = { gain_linear_max };
    amplitude_clip_hard(&ac_out, &ac_in, &ac_cfg, NULL);

    // 5. smooth: amplitude_smooth
    amplitude_smooth_out_t    as_out = { smooth_gain };
    amplitude_smooth_in_t     as_in  = { clamped_gain };
    amplitude_smooth_params_t as_cfg = { 0.020f, 0.100f, (float)params.sample_rate };
    amplitude_smooth(&as_out, &as_in, &as_cfg, (amplitude_smooth_state_t *)&state->smooth_state);

    // 6. gate_gain: amplitude_multiply
    amplitude_multiply_out_t am1_out = { gated_gain };
    amplitude_multiply_in_t  am1_in  = { smooth_gain, gate };
    amplitude_multiply(&am1_out, &am1_in, NULL, NULL);

    // 7. apply_gain: amplitude_multiply
    amplitude_multiply_out_t am2_out = { sustained };
    amplitude_multiply_in_t  am2_in  = { in.signal, gated_gain };
    amplitude_multiply(&am2_out, &am2_in, NULL, NULL);

    // 8. tone: filter_biquad
    filter_biquad_params_t bq = calculate_lpf_coeffs(8000.0f, 0.707f, (float)params.sample_rate);
    filter_biquad_out_t fb_out = { filtered };
    filter_biquad_in_t  fb_in  = { sustained };
    filter_biquad(&fb_out, &fb_in, &bq, (filter_biquad_state_t *)&state->biquad_state);

    // 9. trim: amplitude_multiply
    float unity[CHUNK_LENGTH];
    for (int i = 0; i < CHUNK_LENGTH; i++) unity[i] = 1.0f;
    amplitude_multiply_out_t am3_out = { trimmed };
    amplitude_multiply_in_t  am3_in  = { filtered, unity };
    amplitude_multiply(&am3_out, &am3_in, NULL, NULL);

    // 10. blend: mix_wet_dry
    mix_wet_dry_out_t    mw_out = { out.signal };
    mix_wet_dry_in_t     mw_in  = { in.signal, trimmed };
    mix_wet_dry_params_t mw_cfg = { 1.0f };
    mix_wet_dry(&mw_out, &mw_in, &mw_cfg, NULL);
}
