#include <apgcore/compiler_v2.h>
#include <apgcore/runtime_v2.h>
#include <apgcore/unit_v2.h>

#include "test_runtime_v2_harness.h"

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

static int load_runtime(uc_arena *arena, apg_v2_runtime_t *runtime) {
    apg_unit_v2_t          unit;
    apg_v2_compiled_unit_t plan;
    uc_error               err    = {0};
    uc_status              status = apg_unit_v2_load_file("units-v2/simple_gain.unit.v2.yaml", arena, &unit, &err);
    if (status != UC_OK) {
        fprintf(stderr, "load error: %s\n", err.msg);
        return fail("failed to load v2 fixture");
    }

    status = apg_v2_compile_unit(&unit, arena, &plan, &err);
    if (status != UC_OK) {
        fprintf(stderr, "compile error: %s\n", err.msg);
        return fail("failed to compile v2 fixture");
    }

    status = test_apg_v2_runtime_init_image(&plan, TEST_CAPACITY, 48000.0f, arena, runtime, &err);
    if (status != UC_OK) {
        fprintf(stderr, "runtime init error: %s\n", err.msg);
        return fail("failed to initialize v2 runtime");
    }
    return 0;
}

int main(void) {
    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_v2_runtime_t runtime;
    if (load_runtime(&arena, &runtime)) {
        uc_arena_free(&arena);
        return 1;
    }

    float          input[TEST_CAPACITY];
    float          output[TEST_CAPACITY];
    const uint32_t frames[] = {64u, 128u, 256u, 512u};

    for (size_t i = 0; i < sizeof(frames) / sizeof(frames[0]); i++) {
        fill_input(input, frames[i]);
        for (uint32_t k = 0; k < TEST_CAPACITY; k++)
            output[k] = SENTINEL;

        if (!apg_v2_runtime_process_mono_ports(&runtime, "input", input, "output", output, frames[i]))
            return fail("v2 runtime rejected a valid explicit frame count");
        if (assert_output(output, frames[i]))
            return 1;
    }

    for (uint32_t k = 0; k < TEST_CAPACITY; k++)
        output[k] = SENTINEL;
    if (apg_v2_runtime_process_mono_ports(&runtime, "input", input, "output", output, TEST_CAPACITY + 1u))
        return fail("v2 runtime accepted a frame count beyond capacity");
    if (output[0] != SENTINEL)
        return fail("rejected explicit-frame process modified output");

    apg_v2_runtime_destroy(&runtime);
    uc_arena_free(&arena);
    return 0;
}
