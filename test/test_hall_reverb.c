#include <apgcore/compiler/project_compiler_v2.h>
#include <apgcore/measure/measure_v2.h>
#include <apgcore/runtime/runtime_v2.h>

#include "test_runtime_v2_harness.h"

#include <math.h>
#include <stdio.h>

#define CHUNK  512u
#define CHUNKS 48u

static int fail(const char *msg) {
    fprintf(stderr, "FAIL: %s\n", msg);
    return 1;
}

static int load_pedalboard(uc_arena *arena, apg_project_v2_compiled_t *compiled) {
    apg_project_v2_resolved_t project;
    uc_error                  err    = {0};
    uc_status                 status = apg_project_v2_load_resolved_file(
        "test/fixtures/projects-v2/guitar-pedalboard.project.v2.yaml", arena, &project, &err
    );
    if (status == UC_OK)
        status = apg_project_v2_compile(&project, arena, compiled, &err);
    if (status != UC_OK) {
        fprintf(stderr, "pedalboard setup error: %s\n", err.msg);
        return 1;
    }
    return 0;
}

static void fill_guitar_like_input(float *input, uint32_t chunk_index) {
    for (uint32_t i = 0; i < CHUNK; i++) {
        uint32_t n = chunk_index * CHUNK + i;
        float    t = (float)n / 48000.0f;
        float    a = 0.34f * expf(-0.45f * t);
        input[i]   = a * (sinf(2.0f * 3.14159265358979323846f * 110.0f * t) +
                        0.45f * sinf(2.0f * 3.14159265358979323846f * 220.0f * t));
    }
}

static int process_render(apg_v2_runtime_t *runtime, float *peak, double *sum_sq) {
    float input[CHUNK];
    float output[CHUNK];
    float previous_last = 0.0f;

    for (uint32_t chunk = 0; chunk < CHUNKS; chunk++) {
        fill_guitar_like_input(input, chunk);
        if (!test_runtime_process_mono_ports(runtime, "input", input, "output", output, CHUNK)) {
            fprintf(stderr, "pedalboard runtime error: %s\n", apg_v2_measure_last_error(runtime));
            return fail("pedalboard offline render failed");
        }
        if (chunk > 0 && fabsf(output[0] - previous_last) > 3.0f)
            return fail("pedalboard render produced a large chunk-boundary discontinuity");
        previous_last = output[CHUNK - 1u];

        for (uint32_t i = 0; i < CHUNK; i++) {
            if (!isfinite(output[i]))
                return fail("pedalboard render produced non-finite output");
            float a = fabsf(output[i]);
            if (a > *peak)
                *peak = a;
            *sum_sq += (double)output[i] * (double)output[i];
        }
    }
    return 0;
}

int main(void) {
    uc_arena arena;
    if (uc_arena_init(&arena, 1024u * 1024u) != 0)
        return fail("arena init failed");

    apg_project_v2_compiled_t compiled;
    if (load_pedalboard(&arena, &compiled)) {
        uc_arena_free(&arena);
        return 1;
    }

    apg_v2_runtime_t runtime;
    uc_error         err = {0};
    if (test_apg_v2_runtime_init_registry(&compiled.plan, CHUNK, 48000.0f, &arena, &runtime, &err) != UC_OK) {
        fprintf(stderr, "runtime init error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("failed to initialize pedalboard runtime");
    }

    if (!test_runtime_set_param_by_name(&runtime, "drive1.drive", 3.0f) ||
        !test_runtime_set_param_by_name(&runtime, "delay1.mix", 0.45f) ||
        !test_runtime_set_param_by_name(&runtime, "blend1.mix", 0.35f))
        return fail("failed to set pedalboard render params");

    float  peak   = 0.0f;
    double sum_sq = 0.0;
    if (process_render(&runtime, &peak, &sum_sq))
        return 1;

    double rms = sqrt(sum_sq / (double)(CHUNKS * CHUNK));
    if (peak <= 1e-6f || peak > 4.0f)
        return fail("pedalboard render peak is out of range");
    if (rms <= 1e-7 || rms > 1.5)
        return fail("pedalboard render RMS is out of range");

    apg_v2_meter_snapshot_t meter;
    if (!apg_v2_measure_get_output_meter(&runtime, "output", 0u, &meter) || !meter.valid || meter.frames != CHUNK)
        return fail("pedalboard render output meter was not updated");

    apg_v2_runtime_destroy(&runtime);
    uc_arena_free(&arena);
    return 0;
}
