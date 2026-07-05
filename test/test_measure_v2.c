#include <apgcore/compiler/compiler_v2.h>
#include <apgcore/measure/measure_v2.h>
#include <apgcore/runtime/runtime_v2.h>
#include <apgcore/validator/unit_v2.h>

#include "test_runtime_v2_harness.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int fail(const char *msg) {
    fprintf(stderr, "FAIL: %s\n", msg);
    return 1;
}

static int
load_compile_runtime(uc_arena *arena, apg_unit_v2_t *unit, apg_v2_compiled_unit_t *plan, apg_v2_runtime_t *runtime) {
    uc_error  err    = {0};
    uc_status status = apg_unit_v2_load_file("test/fixtures/units-v2/simple_gain.unit.v2.yaml", arena, unit, &err);
    if (status != UC_OK) {
        fprintf(stderr, "load error: %s\n", err.msg);
        return fail("failed to load fixture");
    }
    status = apg_v2_compile_unit(unit, arena, plan, &err);
    if (status != UC_OK) {
        fprintf(stderr, "compile error: %s\n", err.msg);
        return fail("failed to compile fixture");
    }
    status = test_apg_v2_runtime_init_registry(plan, 8u, 48000.0f, arena, runtime, &err);
    if (status != UC_OK) {
        fprintf(stderr, "runtime error: %s\n", err.msg);
        return fail("failed to initialize runtime");
    }
    return 0;
}

static int test_measure_snapshot_and_meters(void) {
    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_v2_compiled_unit_t plan;
    apg_unit_v2_t          unit;
    apg_v2_runtime_t       runtime;
    if (load_compile_runtime(&arena, &unit, &plan, &runtime)) {
        uc_arena_free(&arena);
        return 1;
    }

    apg_v2_measure_runtime_snapshot_t snapshot;
    if (!apg_v2_measure_runtime_snapshot(&runtime, &snapshot))
        return fail("measure snapshot failed");
    if (snapshot.frame_capacity != 8u || snapshot.sample_rate != 48000.0f || snapshot.signals_len != 3u ||
        snapshot.params_len != 1u || snapshot.nodes_len != 2u || snapshot.input_meters_len != 1u ||
        snapshot.output_meters_len != 1u || snapshot.has_processed)
        return fail("unexpected measure snapshot before process");

    apg_v2_meter_snapshot_t meter;
    if (!apg_v2_measure_get_output_meter(&runtime, "output", 0u, &meter) || meter.valid)
        return fail("unexpected initial output meter");

    float input[4]  = {0.25f, -0.5f, 0.75f, -1.0f};
    float output[4] = {0};
    if (!apg_v2_runtime_process_mono_ports(&runtime, "input", input, "output", output, 4u))
        return fail("runtime process failed");

    if (!apg_v2_measure_runtime_snapshot(&runtime, &snapshot) || !snapshot.has_processed)
        return fail("measure snapshot did not report processed state");
    if (!apg_v2_measure_get_output_meter(&runtime, "output", 0u, &meter) || !meter.valid || meter.frames != 4u ||
        fabsf(meter.rms - 0.6846532f) > 0.00001f)
        return fail("unexpected output meter after process");
    if (apg_v2_measure_get_output_meter(&runtime, "missing", 0u, &meter))
        return fail("measure accepted missing meter");

    apg_v2_runtime_destroy(&runtime);
    uc_arena_free(&arena);
    return 0;
}

static int test_measure_last_error(void) {
    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_v2_compiled_unit_t plan;
    apg_unit_v2_t          unit;
    apg_v2_runtime_t       runtime;
    if (load_compile_runtime(&arena, &unit, &plan, &runtime)) {
        uc_arena_free(&arena);
        return 1;
    }

    float input[4]  = {0};
    float output[4] = {0};
    if (apg_v2_runtime_process_mono_ports(&runtime, "missing", input, "output", output, 4u))
        return fail("runtime accepted missing input port");
    const char *error = apg_v2_measure_last_error(&runtime);
    if (!error || !strstr(error, "input audio port"))
        return fail("measure did not expose runtime error");

    apg_v2_runtime_destroy(&runtime);
    uc_arena_free(&arena);
    return 0;
}

static int test_measure_snapshot_is_non_mutating(void) {
    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_v2_compiled_unit_t plan;
    apg_unit_v2_t          unit;
    apg_v2_runtime_t       runtime;
    if (load_compile_runtime(&arena, &unit, &plan, &runtime)) {
        uc_arena_free(&arena);
        return 1;
    }

    apg_v2_measure_runtime_snapshot_t snapshot_before;
    if (!apg_v2_measure_runtime_snapshot(&runtime, &snapshot_before)) {
        apg_v2_runtime_destroy(&runtime);
        uc_arena_free(&arena);
        return fail("initial snapshot failed");
    }

    float input[4]  = {0.25f, -0.5f, 0.75f, -1.0f};
    float output[4] = {0};
    if (apg_v2_runtime_process_mono_ports(&runtime, "missing", input, "output", output, 4u)) {
        apg_v2_runtime_destroy(&runtime);
        uc_arena_free(&arena);
        return fail("runtime accepted missing input port");
    }

    const char *runtime_error = apg_v2_measure_last_error(&runtime);
    if (!runtime_error || !strstr(runtime_error, "input audio port")) {
        apg_v2_runtime_destroy(&runtime);
        uc_arena_free(&arena);
        return fail("runtime error message not set");
    }

    apg_v2_measure_runtime_snapshot_t snapshot_after_error;
    if (!apg_v2_measure_runtime_snapshot(&runtime, &snapshot_after_error)) {
        apg_v2_runtime_destroy(&runtime);
        uc_arena_free(&arena);
        return fail("snapshot failed after runtime error");
    }

    apg_v2_meter_snapshot_t meter;
    if (!apg_v2_measure_get_output_meter(&runtime, "output", 0u, &meter) || meter.valid) {
        apg_v2_runtime_destroy(&runtime);
        uc_arena_free(&arena);
        return fail("measure should report invalid output meter");
    }

    const char *metric = apg_v2_measure_last_error(&runtime);
    if (!metric || strcmp(metric, runtime_error) != 0) {
        apg_v2_runtime_destroy(&runtime);
        uc_arena_free(&arena);
        return fail("measure path changed runtime error text");
    }

    if (snapshot_before.frame_capacity != snapshot_after_error.frame_capacity ||
        snapshot_before.sample_rate != snapshot_after_error.sample_rate ||
        snapshot_before.signals_len != snapshot_after_error.signals_len ||
        snapshot_before.params_len != snapshot_after_error.params_len ||
        snapshot_before.nodes_len != snapshot_after_error.nodes_len ||
        snapshot_before.input_meters_len != snapshot_after_error.input_meters_len ||
        snapshot_before.output_meters_len != snapshot_after_error.output_meters_len ||
        snapshot_before.project_muted != snapshot_after_error.project_muted) {
        apg_v2_runtime_destroy(&runtime);
        uc_arena_free(&arena);
        return fail("snapshot metadata changed across non-mutating reads");
    }

    apg_v2_runtime_destroy(&runtime);
    uc_arena_free(&arena);
    return 0;
}

int main(void) {
    if (test_measure_snapshot_and_meters())
        return 1;
    if (test_measure_last_error())
        return 1;
    return test_measure_snapshot_is_non_mutating();
}
