#include <apgcore/measure_v2.h>
#include <apgcore/project_compiler_v2.h>
#include <apgcore/runtime_v2.h>

#include "test_runtime_v2_harness.h"

#include <math.h>
#include <stdio.h>

#define TEST_CHUNK 512u

static int fail(const char *msg) {
    fprintf(stderr, "FAIL: %s\n", msg);
    return 1;
}

static int load_compile_project(
    const char *path, uc_arena *arena, apg_project_v2_resolved_t *project, apg_project_v2_compiled_t *compiled
) {
    uc_error  err    = {0};
    uc_status status = apg_project_v2_load_resolved_file(path, arena, project, &err);
    if (status == UC_OK)
        status = apg_project_v2_compile(project, arena, compiled, &err);
    if (status != UC_OK) {
        fprintf(stderr, "project setup error: %s\n", err.msg);
        return 1;
    }
    return 0;
}

static int process_chunk(apg_v2_runtime_t *runtime, int chunk, float *output, float *previous_last, double *sum_sq) {
    float input[TEST_CHUNK];
    for (size_t i = 0; i < TEST_CHUNK; i++) {
        size_t n = (size_t)chunk * TEST_CHUNK + i;
        input[i] = 0.15f * sinf(2.0f * 3.14159265358979323846f * 110.0f * (float)n / 48000.0f);
    }

    if (!apg_v2_runtime_process_mono_ports(runtime, "input", input, "output", output, TEST_CHUNK)) {
        fprintf(stderr, "runtime error: %s\n", apg_v2_measure_last_error(runtime));
        return fail("v2 offline chain processing failed");
    }

    for (size_t i = 0; i < TEST_CHUNK; i++) {
        if (!isfinite(output[i]))
            return fail("v2 offline chain produced non-finite output");
        *sum_sq += (double)output[i] * (double)output[i];
    }

    if (chunk > 0 && fabsf(output[0] - *previous_last) > 4.0f)
        return fail("v2 offline chain produced a large chunk-boundary discontinuity");
    *previous_last = output[TEST_CHUNK - 1u];
    return 0;
}

int main(void) {
    uc_arena arena;
    if (uc_arena_init(&arena, 1024u * 1024u) != 0)
        return fail("arena init failed");

    apg_project_v2_resolved_t project;
    apg_project_v2_compiled_t compiled;
    if (load_compile_project("test/fixtures/projects-v2/two-gain-chain.project.v2.yaml", &arena, &project, &compiled)) {
        uc_arena_free(&arena);
        return 1;
    }

    apg_v2_runtime_t runtime;
    uc_error         err = {0};
    if (test_apg_v2_runtime_init_registry(&compiled.plan, TEST_CHUNK, 48000.0f, &arena, &runtime, &err) != UC_OK) {
        fprintf(stderr, "runtime init error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("failed to initialize v2 offline chain");
    }

    if (!apg_v2_runtime_set_param(&runtime, "gain1.gain", 1.5f) ||
        !apg_v2_runtime_set_param(&runtime, "gain2.gain", 2.0f))
        return fail("failed to set v2 offline chain params");

    float  output[TEST_CHUNK];
    float  previous_last = 0.0f;
    float  peak          = 0.0f;
    double sum_sq        = 0.0;
    for (int chunk = 0; chunk < 64; chunk++) {
        if (process_chunk(&runtime, chunk, output, &previous_last, &sum_sq))
            return 1;
        for (size_t i = 0; i < TEST_CHUNK; i++) {
            float a = fabsf(output[i]);
            if (a > peak)
                peak = a;
        }
    }

    double rms = sqrt(sum_sq / (double)(64u * TEST_CHUNK));
    if (peak <= 1e-7f || peak > 0.6f)
        return fail("v2 offline chain peak is out of range");
    if (rms <= 1e-8 || rms > 0.43)
        return fail("v2 offline chain RMS is out of range");

    apg_v2_meter_snapshot_t meter;
    if (!apg_v2_measure_get_output_meter(&runtime, "output", 0u, &meter) || !meter.valid || meter.frames != TEST_CHUNK)
        return fail("v2 offline chain output meter was not updated");

    apg_v2_runtime_destroy(&runtime);
    uc_arena_free(&arena);
    return 0;
}
