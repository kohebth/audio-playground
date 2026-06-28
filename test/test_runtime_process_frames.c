#include <runtime.h>

#include <math.h>
#include <stdbool.h>
#include <stdio.h>

#define TEST_CAPACITY 512
#define SENTINEL      12345.0f

static int fail(const char *msg) {
    fprintf(stderr, "FAIL: %s\n", msg);
    return 1;
}

static void fill_input(float *input, uint32_t frames) {
    for (uint32_t i = 0; i < TEST_CAPACITY; i++)
        input[i] = SENTINEL;
    for (uint32_t i = 0; i < frames; i++)
        input[i] = 0.1f * sinf(2.0f * 3.14159265358979323846f * 220.0f * (float)i / 48000.0f);
}

static int assert_output(const float *output, uint32_t frames) {
    float peak = 0.0f;
    for (uint32_t i = 0; i < frames; i++) {
        if (!isfinite(output[i]))
            return fail("explicit-frame process produced non-finite output");
        const float a = fabsf(output[i]);
        if (a > peak)
            peak = a;
    }
    if (peak > 32.0f)
        return fail("explicit-frame process produced excessive peak");

    for (uint32_t i = frames; i < TEST_CAPACITY; i++) {
        if (output[i] != SENTINEL)
            return fail("explicit-frame process wrote past requested frame count");
    }
    return 0;
}

int main(void) {
    runtime_context_t ctx  = {.sample_rate = 48000.0f, .chunk_length = TEST_CAPACITY};
    runtime_unit_t   *unit = runtime_unit_load("units/analog_delay.unit.yaml", ctx);
    if (!unit)
        return fail("failed to load analog_delay unit");

    float          input[TEST_CAPACITY];
    float          output[TEST_CAPACITY];
    const uint32_t frames[] = {64u, 128u, 256u, 512u};

    for (size_t i = 0; i < sizeof(frames) / sizeof(frames[0]); i++) {
        fill_input(input, frames[i]);
        for (uint32_t k = 0; k < TEST_CAPACITY; k++)
            output[k] = SENTINEL;

        if (!runtime_unit_process_frames(unit, input, output, frames[i]))
            return fail("runtime_unit_process_frames rejected a valid frame count");
        if (assert_output(output, frames[i]))
            return 1;
    }

    for (uint32_t k = 0; k < TEST_CAPACITY; k++)
        output[k] = SENTINEL;
    if (runtime_unit_process_frames(unit, input, output, TEST_CAPACITY + 1u))
        return fail("runtime_unit_process_frames accepted a frame count beyond capacity");
    if (output[0] != SENTINEL)
        return fail("rejected explicit-frame process modified output");

    runtime_unit_destroy(unit);
    return 0;
}
