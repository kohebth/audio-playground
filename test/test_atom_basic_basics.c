#include "test_atom_basic_common.h"

int test_amplitude_multiply(void) {
    float a[TEST_CHUNK];
    float b[TEST_CHUNK];
    float y[TEST_CHUNK];

    for (int i = 0; i < TEST_CHUNK; i++) {
        a[i] = (float)i * 0.001f;
        b[i] = 0.25f;
        y[i] = 0.0f;
    }

    amplitude_multiply_out_t    out = {.signal = y};
    amplitude_multiply_in_t     in  = {.signal_a = a, .signal_b = b};
    amplitude_multiply_params_t params;
    amplitude_multiply_state_t  state;
    const apg_process_info_t    info = {
           .sample_rate = 48000.0f, .frames = TEST_CHUNK, .output_frames = TEST_CHUNK, .channels = 1u
    };
    amplitude_multiply_process(&out, &in, &params, &state, &info);

    for (int i = 0; i < TEST_CHUNK; i++) {
        float expected = a[i] * b[i];
        if (fabsf(y[i] - expected) > 1e-7f)
            return fail("amplitude_multiply mismatch");
    }
    return 0;
}

int test_soft_clip_bounds_and_monotonicity(void) {
    float x[TEST_CHUNK];
    float y[TEST_CHUNK];

    for (int i = 0; i < TEST_CHUNK; i++) {
        x[i] = -4.0f + 8.0f * (float)i / (float)(TEST_CHUNK - 1);
        y[i] = 0.0f;
    }

    amplitude_clip_soft_out_t    out    = {.signal = y};
    amplitude_clip_soft_in_t     in     = {.signal = x};
    amplitude_clip_soft_params_t params = {.threshold = 0.8f, .curve = 1};
    amplitude_clip_soft_state_t  state;
    const apg_process_info_t     info = {
            .sample_rate = 48000.0f, .frames = TEST_CHUNK, .output_frames = TEST_CHUNK, .channels = 1u
    };
    amplitude_clip_soft_process(&out, &in, &params, &state, &info);

    if (assert_finite_buffer(y, TEST_CHUNK, "amplitude_clip_soft"))
        return 1;

    for (int i = 0; i < TEST_CHUNK; i++) {
        if (fabsf(y[i]) > params.threshold + 1e-4f)
            return fail("amplitude_clip_soft exceeded threshold");
        if (i > 0 && y[i] + 1e-6f < y[i - 1])
            return fail("amplitude_clip_soft is not monotonic");
    }
    return 0;
}

int test_delay_line_impulse_position(void) {
    float        x[TEST_CHUNK];
    float        y[TEST_CHUNK];
    static float buffer[192000];
    memset(x, 0, sizeof(x));
    memset(y, 0, sizeof(y));
    memset(buffer, 0, sizeof(buffer));
    x[0] = 1.0f;

    delay_line_out_t         out    = {.signal = y};
    delay_line_in_t          in     = {.signal = x};
    delay_line_params_t      params = {.length = 12};
    delay_line_state_t       state  = {.buffer = buffer, .write_pos = 0};
    const apg_process_info_t info   = {
          .sample_rate = 48000.0f, .frames = TEST_CHUNK, .output_frames = TEST_CHUNK, .channels = 1u
    };
    delay_line_process(&out, &in, &params, &state, &info);

    for (int i = 0; i < TEST_CHUNK; i++) {
        float expected = (i == 12) ? 1.0f : 0.0f;
        if (fabsf(y[i] - expected) > 1e-6f)
            return fail("delay_line impulse offset mismatch");
    }
    return 0;
}

int test_delay_extreme_bounds(void) {
    float        x[8] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float        y[8] = {0};
    static float buffer[192000];
    memset(buffer, 0, sizeof(buffer));

    delay_line_out_t    out    = {.signal = y};
    delay_line_in_t     in     = {.signal = x};
    delay_line_params_t params = {.length = 192000};
    delay_line_state_t  state  = {.buffer = buffer, .write_pos = -1};
    apg_process_info_t  info   = {.sample_rate = 48000.0f, .frames = 8u, .output_frames = 8u, .channels = 1u};

    delay_line_process(&out, &in, &params, &state, &info);
    if (assert_finite_buffer(y, 8, "delay_line extreme bounds"))
        return 1;
    if (state.write_pos < 0 || state.write_pos >= 192000)
        return fail("delay_line write position was not normalized");

    memset(y, 0, sizeof(y));
    memset(buffer, 0, sizeof(buffer));
    delay_fractional_out_t    frac_out    = {.signal = y};
    delay_fractional_in_t     frac_in     = {.signal = x};
    delay_fractional_params_t frac_params = {.delay_samples = NAN, .interpolation = 0};
    delay_fractional_state_t  frac_state  = {.buffer = buffer, .write_pos = -3};
    delay_fractional_process(&frac_out, &frac_in, &frac_params, &frac_state, &info);
    if (assert_finite_buffer(y, 8, "delay_fractional extreme bounds"))
        return 1;
    if (frac_state.write_pos < 0 || frac_state.write_pos >= 192000)
        return fail("delay_fractional write position was not normalized");
    return 0;
}

int test_biquad_impulse_stability(void) {
    float x[TEST_CHUNK];
    float y[TEST_CHUNK];
    memset(x, 0, sizeof(x));
    memset(y, 0, sizeof(y));
    x[0] = 1.0f;

    filter_biquad_coefficients_out_t    out    = {.signal = y};
    filter_biquad_coefficients_in_t     in     = {.signal = x};
    filter_biquad_coefficients_params_t params = {
        .b0 = 0.30f,
        .b1 = 0.30f,
        .b2 = 0.0f,
        .a1 = -0.40f,
        .a2 = 0.0f,
    };
    filter_biquad_coefficients_state_t state = {0};
    const apg_process_info_t           info  = {
                   .sample_rate = 48000.0f, .frames = TEST_CHUNK, .output_frames = TEST_CHUNK, .channels = 1u
    };
    filter_biquad_coefficients_process(&out, &in, &params, &state, &info);

    if (assert_finite_buffer(y, TEST_CHUNK, "filter_biquad"))
        return 1;

    float peak = 0.0f;
    for (int i = 0; i < TEST_CHUNK; i++) {
        if (fabsf(y[i]) > peak)
            peak = fabsf(y[i]);
    }
    if (peak <= 0.0f || peak > 2.0f)
        return fail("filter_biquad impulse peak out of expected range");
    if (fabsf(y[TEST_CHUNK - 1]) > 1e-4f)
        return fail("filter_biquad impulse did not decay");
    return 0;
}

int test_biquad_invalid_coefficients_bypass(void) {
    float x[8] = {0.25f, -0.5f, 1.0f, 0.0f, 0.2f, -0.1f, 0.4f, -0.3f};
    float y[8] = {0};

    filter_biquad_coefficients_out_t    out    = {.signal = y};
    filter_biquad_coefficients_in_t     in     = {.signal = x};
    filter_biquad_coefficients_params_t params = {.b0 = NAN, .b1 = 0.0f, .b2 = 0.0f, .a1 = 0.0f, .a2 = 2.0f};
    filter_biquad_coefficients_state_t  state  = {.z1 = 1.0f, .z2 = 1.0f};
    apg_process_info_t info = {.sample_rate = 48000.0f, .frames = 8u, .output_frames = 8u, .channels = 1u};

    filter_biquad_coefficients_process(&out, &in, &params, &state, &info);
    for (int i = 0; i < 8; ++i) {
        if (y[i] != x[i])
            return fail("invalid biquad coefficients did not bypass input");
    }
    if (state.z1 != 0.0f || state.z2 != 0.0f)
        return fail("invalid biquad coefficients did not reset state");
    return 0;
}

int test_biquad_cutoff_smoothing(void) {
    float x[TEST_CHUNK];
    float cutoff[TEST_CHUNK];
    float y[TEST_CHUNK];

    for (int i = 0; i < TEST_CHUNK; i++) {
        x[i]      = (i == 0) ? 1.0f : 0.0f;
        cutoff[i] = i < (TEST_CHUNK / 2) ? 400.0f : 6000.0f;
        y[i]      = 0.0f;
    }

    filter_biquad_out_t    out    = {.signal = y};
    filter_biquad_in_t     in     = {.signal = x, .cutoff = cutoff};
    filter_biquad_params_t params = {
        .cutoff       = 400.0f,
        .q            = 0.70710678f,
        .mode         = 0,
        .sample_rate  = 8000.0f,
        .smoothing_ms = 5.0f,
    };
    filter_biquad_state_t state = {0};
    apg_process_info_t    info  = {
            .sample_rate = 48000.0f, .frames = TEST_CHUNK, .output_frames = TEST_CHUNK, .channels = 1u
    };

    filter_biquad_process(&out, &in, &params, &state, &info);

    if (assert_finite_buffer(y, TEST_CHUNK, "filter_biquad"))
        return 1;
    if (state.current_cutoff <= 400.0f || state.current_cutoff >= 6000.0f)
        return fail("filter_biquad cutoff did not smooth toward target");
    if (state.current_q <= 0.0f)
        return fail("filter_biquad q state was not initialized");
    return 0;
}

int test_biquad_modes_are_finite(void) {
    float x[TEST_CHUNK];
    float y[TEST_CHUNK];

    for (int mode = 0; mode <= 3; mode++) {
        memset(x, 0, sizeof(x));
        memset(y, 0, sizeof(y));
        x[0] = 1.0f;

        filter_biquad_out_t    out    = {.signal = y};
        filter_biquad_in_t     in     = {.signal = x, .cutoff = NULL};
        filter_biquad_params_t params = {
            .cutoff       = 1200.0f,
            .q            = 0.70710678f,
            .mode         = mode,
            .sample_rate  = 48000.0f,
            .smoothing_ms = 0.0f,
        };
        filter_biquad_state_t    state = {0};
        const apg_process_info_t info  = {
             .sample_rate = 48000.0f, .frames = TEST_CHUNK, .output_frames = TEST_CHUNK, .channels = 1u
        };

        filter_biquad_process(&out, &in, &params, &state, &info);

        if (assert_finite_buffer(y, TEST_CHUNK, "filter_biquad mode"))
            return 1;
        if (state.current_b0 == 0.0f && state.current_b1 == 0.0f && state.current_b2 == 0.0f)
            return fail("filter_biquad mode coefficients were not initialized");
    }
    return 0;
}

int test_runtime_sample_rate_overrides_legacy_params(void) {
    float                   y[4] = {0};
    generation_lfo_out_t    out  = {.signal = y};
    generation_lfo_in_t     in;
    generation_lfo_params_t params = {
        .frequency = 100.0f, .waveform = WAVEFORM_SINE, .phase_offset = 0.0f, .sample_rate = 1000.0f
    };
    generation_lfo_state_t state = {0};
    apg_process_info_t     info  = {.sample_rate = 10000.0f, .frames = 4u, .output_frames = 4u, .channels = 1u};

    generation_lfo_process(&out, &in, &params, &state, &info);
    if (fabsf(state.phase - 0.04f) > 1e-6f)
        return fail("LFO did not use runtime sample rate");

    generation_impulse_out_t    impulse_out = {.signal = y};
    generation_impulse_in_t     impulse_in;
    generation_impulse_params_t impulse_params = {.interval = 0.001f, .sample_rate = 1000.0f};
    generation_impulse_state_t  impulse_state  = {0};
    generation_impulse_process(&impulse_out, &impulse_in, &impulse_params, &impulse_state, &info);
    if (impulse_state.counter != 6)
        return fail("impulse generator did not use runtime sample rate");
    return 0;
}
