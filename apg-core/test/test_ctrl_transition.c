#include <apgcore/compiler/compiler_v2.h>
#include <apgcore/runtime/runtime_v2.h>
#include <apgcore/validator/unit_v2.h>

#include "test_runtime_v2_harness.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define TEST_CHUNK 512u

static int fail(const char *msg) {
    fprintf(stderr, "FAIL: %s\n", msg);
    return 1;
}

static int load_control_runtime(uc_arena *arena, apg_v2_runtime_t *runtime) {
    const char *yaml = "kind: apg.unit\n"
                       "schema: apg.unit.v2\n"
                       "name: gain_control_transition\n"
                       "version: 2.0.0\n"
                       "params:\n"
                       "  gain:\n"
                       "    type: float\n"
                       "    default: 1.0\n"
                       "    min: 0.0\n"
                       "    max: 4.0\n"
                       "    smoothing_ms: 60\n"
                       "ports:\n"
                       "  inputs:\n"
                       "    - name: input\n"
                       "      type: audio\n"
                       "      channels: 1\n"
                       "    - name: amount\n"
                       "      type: control\n"
                       "      target:\n"
                       "        kind: param\n"
                       "        name: gain\n"
                       "  outputs:\n"
                       "    - name: output\n"
                       "      type: audio\n"
                       "      channels: 1\n"
                       "graph:\n"
                       "  signals:\n"
                       "    - input\n"
                       "    - output\n"
                       "    - gain_value\n"
                       "  nodes:\n"
                       "    - id: gain_value\n"
                       "      atom: generation_dc\n"
                       "      out:\n"
                       "        signal: gain_value\n"
                       "      config:\n"
                       "        value: ${params.gain}\n"
                       "    - id: apply_gain\n"
                       "      atom: amplitude_multiply\n"
                       "      in:\n"
                       "        signal_a: input\n"
                       "        signal_b: gain_value\n"
                       "      out:\n"
                       "        signal: output\n"
                       "compatibility:\n"
                       "  desktop_full: true\n";

    apg_unit_v2_t unit;
    uc_error      err    = {0};
    uc_status     status = apg_unit_v2_load_string(yaml, strlen(yaml), arena, &unit, &err);
    if (status != UC_OK) {
        fprintf(stderr, "load error: %s\n", err.msg);
        return fail("failed to load control transition unit");
    }

    apg_v2_compiled_unit_t plan;
    status = apg_v2_compile_unit(&unit, arena, &plan, &err);
    if (status != UC_OK) {
        fprintf(stderr, "compile error: %s\n", err.msg);
        return fail("failed to compile control transition unit");
    }

    status = test_apg_v2_runtime_init_registry(&plan, TEST_CHUNK, 48000.0f, arena, runtime, &err);
    if (status != UC_OK) {
        fprintf(stderr, "runtime init error: %s\n", err.msg);
        return fail("failed to initialize control transition runtime");
    }
    return 0;
}

int main(void) {
    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_v2_runtime_t runtime;
    if (load_control_runtime(&arena, &runtime)) {
        uc_arena_free(&arena);
        return 1;
    }

    float input[TEST_CHUNK];
    float output[TEST_CHUNK];
    for (uint32_t i = 0; i < TEST_CHUNK; i++) {
        input[i]  = 0.15f * sinf(2.0f * 3.14159265358979323846f * 220.0f * (float)i / 48000.0f);
        output[i] = 0.0f;
    }

    if (!test_runtime_process_mono_ports(&runtime, "input", input, "output", output, TEST_CHUNK))
        return fail("initial v2 control transition processing failed");
    if (!test_runtime_set_control_port_by_name(&runtime, "amount", 3.0f))
        return fail("failed to set v2 control target");
    if (runtime.param_targets[0] != 3.0f || runtime.param_smoothing_remaining_frames[0] != 2880u)
        return fail("v2 control transition did not capture target and duration");

    float previous = runtime.params[0];
    for (int i = 0; i < 6; i++) {
        if (!test_runtime_process_mono_ports(&runtime, "input", input, "output", output, TEST_CHUNK))
            return fail("v2 control transition processing failed");
        if (!isfinite(runtime.params[0]))
            return fail("v2 control transition became non-finite");
        if (runtime.params[0] + 1e-6f < previous)
            return fail("v2 control transition was not monotonic");
        if (runtime.params[0] > 3.0f + 1e-4f)
            return fail("v2 control transition overshot target");
        previous = runtime.params[0];
    }

    if (fabsf(runtime.params[0] - 3.0f) > 0.0001f || runtime.param_smoothing_remaining_frames[0] != 0u)
        return fail("v2 control transition did not reach target");
    for (uint32_t i = 0; i < TEST_CHUNK; i++) {
        if (!isfinite(output[i]))
            return fail("v2 control transition produced non-finite output");
    }

    apg_v2_runtime_destroy(&runtime);
    uc_arena_free(&arena);
    return 0;
}
