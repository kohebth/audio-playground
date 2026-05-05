#include <atom/dsp_atoms.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

#define TEST_CHUNK 512

static int fail(const char *msg) {
    fprintf(stderr, "FAIL: %s\n", msg);
    return 1;
}

static int assert_finite_buffer(const float *x, int n, const char *label) {
    for (int i = 0; i < n; i++) {
        if (!isfinite(x[i])) {
            fprintf(stderr, "FAIL: %s produced non-finite sample at %d\n", label, i);
            return 1;
        }
    }
    return 0;
}

static int test_amplitude_multiply(void) {
    float a[TEST_CHUNK];
    float b[TEST_CHUNK];
    float y[TEST_CHUNK];

    for (int i = 0; i < TEST_CHUNK; i++) {
        a[i] = (float)i * 0.001f;
        b[i] = 0.25f;
        y[i] = 0.0f;
    }

    amplitude_multiply_out_t out = {.signal = y};
    amplitude_multiply_in_t in = {.signal_a = a, .signal_b = b};
    amplitude_multiply_params_t params;
    amplitude_multiply_state_t state;
    amplitude_multiply(&out, &in, &params, &state);

    for (int i = 0; i < TEST_CHUNK; i++) {
        float expected = a[i] * b[i];
        if (fabsf(y[i] - expected) > 1e-7f) return fail("amplitude_multiply mismatch");
    }
    return 0;
}

static int test_soft_clip_bounds_and_monotonicity(void) {
    float x[TEST_CHUNK];
    float y[TEST_CHUNK];

    for (int i = 0; i < TEST_CHUNK; i++) {
        x[i] = -4.0f + 8.0f * (float)i / (float)(TEST_CHUNK - 1);
        y[i] = 0.0f;
    }

    amplitude_clip_soft_out_t out = {.signal = y};
    amplitude_clip_soft_in_t in = {.signal = x};
    amplitude_clip_soft_params_t params = {.threshold = 0.8f, .curve = 1};
    amplitude_clip_soft_state_t state;
    amplitude_clip_soft(&out, &in, &params, &state);

    if (assert_finite_buffer(y, TEST_CHUNK, "amplitude_clip_soft")) return 1;

    for (int i = 0; i < TEST_CHUNK; i++) {
        if (fabsf(y[i]) > params.threshold + 1e-4f) return fail("amplitude_clip_soft exceeded threshold");
        if (i > 0 && y[i] + 1e-6f < y[i - 1]) return fail("amplitude_clip_soft is not monotonic");
    }
    return 0;
}

static int test_delay_line_impulse_position(void) {
    float x[TEST_CHUNK];
    float y[TEST_CHUNK];
    static float buffer[192000];
    memset(x, 0, sizeof(x));
    memset(y, 0, sizeof(y));
    memset(buffer, 0, sizeof(buffer));
    x[0] = 1.0f;

    delay_line_out_t out = {.signal = y};
    delay_line_in_t in = {.signal = x};
    delay_line_params_t params = {.length = 12};
    delay_line_state_t state = {.buffer = buffer, .write_pos = 0};
    delay_line(&out, &in, &params, &state);

    for (int i = 0; i < TEST_CHUNK; i++) {
        float expected = (i == 12) ? 1.0f : 0.0f;
        if (fabsf(y[i] - expected) > 1e-6f) return fail("delay_line impulse offset mismatch");
    }
    return 0;
}

static int test_biquad_impulse_stability(void) {
    float x[TEST_CHUNK];
    float y[TEST_CHUNK];
    memset(x, 0, sizeof(x));
    memset(y, 0, sizeof(y));
    x[0] = 1.0f;

    filter_biquad_out_t out = {.signal = y};
    filter_biquad_in_t in = {.signal = x};
    filter_biquad_params_t params = {
        .b0 = 0.30f,
        .b1 = 0.30f,
        .b2 = 0.0f,
        .a1 = -0.40f,
        .a2 = 0.0f,
    };
    filter_biquad_state_t state = {0};
    filter_biquad(&out, &in, &params, &state);

    if (assert_finite_buffer(y, TEST_CHUNK, "filter_biquad")) return 1;

    float peak = 0.0f;
    for (int i = 0; i < TEST_CHUNK; i++) {
        if (fabsf(y[i]) > peak) peak = fabsf(y[i]);
    }
    if (peak <= 0.0f || peak > 2.0f) return fail("filter_biquad impulse peak out of expected range");
    if (fabsf(y[TEST_CHUNK - 1]) > 1e-4f) return fail("filter_biquad impulse did not decay");
    return 0;
}

int main(void) {
    if (test_amplitude_multiply()) return 1;
    if (test_soft_clip_bounds_and_monotonicity()) return 1;
    if (test_delay_line_impulse_position()) return 1;
    if (test_biquad_impulse_stability()) return 1;
    return 0;
}
