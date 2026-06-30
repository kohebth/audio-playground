#include <apgcore/project_compiler_v2.h>
#include <apgcore/runtime_v2.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

static int fail(const char *msg) {
    fprintf(stderr, "FAIL: %s\n", msg);
    return 1;
}

static int load_resolved_project(const char *path, uc_arena *arena, apg_project_v2_resolved_t *project) {
    uc_error  err    = {0};
    uc_status status = apg_project_v2_load_resolved_file(path, arena, project, &err);
    if (status != UC_OK) {
        fprintf(stderr, "project load error: %s\n", err.msg);
        return fail("failed to load resolved project");
    }
    return 0;
}

static int compile_resolved_project(
    const apg_project_v2_resolved_t *project, uc_arena *arena, apg_project_v2_compiled_t *compiled
) {
    uc_error  err    = {0};
    uc_status status = apg_project_v2_compile(project, arena, compiled, &err);
    if (status != UC_OK) {
        fprintf(stderr, "project compile error: %s\n", err.msg);
        return fail("failed to compile project");
    }
    return 0;
}

static int expect_samples(const float *actual, const float *expected, size_t frames, const char *label) {
    for (size_t i = 0; i < frames; i++) {
        if (actual[i] != expected[i]) {
            fprintf(stderr, "unexpected %s sample at %zu: got %f expected %f\n", label, i, actual[i], expected[i]);
            return 1;
        }
    }
    return 0;
}

static int
expect_samples_near(const float *actual, const float *expected, size_t frames, float tolerance, const char *label) {
    for (size_t i = 0; i < frames; i++) {
        float diff = actual[i] > expected[i] ? actual[i] - expected[i] : expected[i] - actual[i];
        if (diff > tolerance) {
            fprintf(stderr, "unexpected %s sample at %zu: got %f expected %f\n", label, i, actual[i], expected[i]);
            return 1;
        }
    }
    return 0;
}

static int expect_finite_samples(const float *samples, size_t frames, const char *label) {
    for (size_t i = 0; i < frames; i++) {
        if (!isfinite(samples[i])) {
            fprintf(stderr, "unexpected non-finite %s sample at %zu: %f\n", label, i, samples[i]);
            return 1;
        }
    }
    return 0;
}

static int expect_meter_near(
    const apg_v2_meter_snapshot_t *meter,
    float                          expected_peak,
    float                          expected_rms,
    uint32_t                       expected_frames,
    const char                    *label
) {
    if (!meter || !meter->valid || meter->frames != expected_frames) {
        fprintf(stderr, "unexpected %s meter validity or frame count\n", label);
        return 1;
    }
    float peak_diff = meter->peak > expected_peak ? meter->peak - expected_peak : expected_peak - meter->peak;
    float rms_diff  = meter->rms > expected_rms ? meter->rms - expected_rms : expected_rms - meter->rms;
    if (peak_diff > 0.00001f || rms_diff > 0.00001f) {
        fprintf(stderr, "unexpected %s meter: peak %f rms %f\n", label, meter->peak, meter->rms);
        return 1;
    }
    return 0;
}

static int test_simple_project_compiles_and_runs(void) {
    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_project_v2_resolved_t project;
    if (load_resolved_project("projects-v2/simple-gain-board.project.v2.yaml", &arena, &project)) {
        uc_arena_free(&arena);
        return 1;
    }

    apg_project_v2_compiled_t compiled;
    if (compile_resolved_project(&project, &arena, &compiled)) {
        uc_arena_free(&arena);
        return 1;
    }

    if (compiled.expanded_unit.params_len != 1u || strcmp(compiled.expanded_unit.params[0].name, "gain1.gain") != 0)
        return fail("project compiler did not namespace params");
    if (strcmp(compiled.expanded_unit.params[0].default_value, "2.0") != 0)
        return fail("project compiler did not apply instance param override");
    if (compiled.expanded_unit.nodes_len != 2u || strcmp(compiled.expanded_unit.nodes[0].id, "gain1.gain_value") != 0)
        return fail("project compiler did not namespace graph nodes");
    if (compiled.plan.unit != &compiled.expanded_unit || compiled.plan.nodes_len != 2u)
        return fail("project compiler did not produce a runtime plan");

    apg_v2_runtime_t runtime;
    uc_error         err    = {0};
    uc_status        status = apg_v2_runtime_init(&compiled.plan, 8u, 48000.0f, &runtime, &err);
    if (status != UC_OK) {
        fprintf(stderr, "runtime init error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("failed to initialize project runtime");
    }

    const float input[3]  = {0.25f, -0.5f, 1.0f};
    float       output[3] = {0.0f, 0.0f, 0.0f};
    if (!apg_v2_runtime_process_mono_ports(&runtime, "input", input, "output", output, 3u)) {
        fprintf(stderr, "runtime error: %s\n", apg_v2_runtime_last_error(&runtime));
        return fail("project runtime processing failed");
    }
    const float expected_default[3] = {0.5f, -1.0f, 2.0f};
    if (expect_samples(output, expected_default, 3u, "project default gain"))
        return 1;

    if (!apg_v2_runtime_set_param(&runtime, "gain1.gain", 3.0f))
        return fail("project runtime did not accept namespaced param");
    if (apg_v2_runtime_set_param(&runtime, "gain", 3.0f))
        return fail("project runtime accepted unqualified param");
    if (!apg_v2_runtime_process_mono_ports(&runtime, "input", input, "output", output, 3u))
        return fail("project runtime processing after param update failed");
    const float expected_updated[3] = {0.5015625f, -1.003125f, 2.00625f};
    if (expect_samples_near(output, expected_updated, 3u, 0.00001f, "project updated gain"))
        return 1;

    apg_v2_runtime_destroy(&runtime);
    uc_arena_free(&arena);
    return 0;
}

static int test_two_instance_project_compiles_and_runs(void) {
    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_project_v2_resolved_t project;
    if (load_resolved_project("projects-v2/two-gain-chain.project.v2.yaml", &arena, &project)) {
        uc_arena_free(&arena);
        return 1;
    }

    apg_project_v2_compiled_t compiled;
    if (compile_resolved_project(&project, &arena, &compiled)) {
        uc_arena_free(&arena);
        return 1;
    }

    if (compiled.expanded_unit.params_len != 2u || strcmp(compiled.expanded_unit.params[0].name, "gain1.gain") != 0 ||
        strcmp(compiled.expanded_unit.params[1].name, "gain2.gain") != 0)
        return fail("two-instance project params were not namespaced");
    if (compiled.expanded_unit.nodes_len != 4u || compiled.plan.nodes_len != 4u)
        return fail("two-instance project did not expand both unit graphs");

    apg_v2_runtime_t runtime;
    uc_error         err    = {0};
    uc_status        status = apg_v2_runtime_init(&compiled.plan, 8u, 48000.0f, &runtime, &err);
    if (status != UC_OK) {
        fprintf(stderr, "runtime init error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("failed to initialize two-instance project runtime");
    }

    apg_v2_meter_snapshot_t meter;
    if (!apg_v2_runtime_get_output_meter(&runtime, "output", 0u, &meter) || meter.valid)
        return fail("two-instance project output meter was not initially empty");
    if (apg_v2_runtime_get_output_meter(&runtime, "missing", 0u, &meter))
        return fail("two-instance project accepted missing output meter");

    const float input[2]  = {0.25f, -0.5f};
    float       output[2] = {0.0f, 0.0f};
    if (!apg_v2_runtime_process_mono_ports(&runtime, "input", input, "output", output, 2u))
        return fail("two-instance project processing failed");
    const float expected_default[2] = {1.5f, -3.0f};
    if (expect_samples(output, expected_default, 2u, "two-instance project default"))
        return 1;
    if (!apg_v2_runtime_get_input_meter(&runtime, "input", 0u, &meter) ||
        expect_meter_near(&meter, 0.5f, 0.3952847f, 2u, "two-instance project input"))
        return 1;
    if (!apg_v2_runtime_get_output_meter(&runtime, "output", 0u, &meter) ||
        expect_meter_near(&meter, 3.0f, 2.3717082f, 2u, "two-instance project output"))
        return 1;

    if (!apg_v2_runtime_set_instance_bypass(&runtime, "gain1", true))
        return fail("two-instance runtime did not accept first instance bypass");
    if (!apg_v2_runtime_process_mono_ports(&runtime, "input", input, "output", output, 2u))
        return fail("two-instance project processing with bypass failed");
    const float expected_bypassed[2] = {0.75f, -1.5f};
    if (expect_samples(output, expected_bypassed, 2u, "two-instance project bypassed"))
        return 1;
    if (!apg_v2_runtime_get_output_meter(&runtime, "output", 0u, &meter) ||
        expect_meter_near(&meter, 1.5f, 1.1858541f, 2u, "two-instance project bypassed output"))
        return 1;

    if (!apg_v2_runtime_set_project_solo(&runtime, true) || !runtime.project_soloed)
        return fail("two-instance runtime did not store project solo state");
    if (!apg_v2_runtime_set_project_mute(&runtime, true))
        return fail("two-instance runtime did not accept project mute");
    if (!apg_v2_runtime_process_mono_ports(&runtime, "input", input, "output", output, 2u))
        return fail("two-instance project processing with mute failed");
    const float expected_muted[2] = {0.0f, 0.0f};
    if (expect_samples(output, expected_muted, 2u, "two-instance project muted"))
        return 1;
    if (!apg_v2_runtime_get_output_meter(&runtime, "output", 0u, &meter) ||
        expect_meter_near(&meter, 0.0f, 0.0f, 2u, "two-instance project muted output"))
        return 1;

    if (!apg_v2_runtime_set_project_mute(&runtime, false) ||
        !apg_v2_runtime_set_instance_bypass(&runtime, "gain1", false))
        return fail("two-instance runtime did not disable project controls");
    if (!apg_v2_runtime_process_mono_ports(&runtime, "input", input, "output", output, 2u))
        return fail("two-instance project processing after disabling controls failed");
    if (expect_samples(output, expected_default, 2u, "two-instance project restored"))
        return 1;

    if (!apg_v2_runtime_set_param(&runtime, "gain2.gain", 4.0f))
        return fail("two-instance runtime did not accept second instance param");
    if (!apg_v2_runtime_process_mono_ports(&runtime, "input", input, "output", output, 2u))
        return fail("two-instance project processing after update failed");
    const float expected_updated[2] = {1.5020833f, -3.0041666f};
    if (expect_samples_near(output, expected_updated, 2u, 0.00001f, "two-instance project updated"))
        return 1;

    apg_v2_runtime_destroy(&runtime);
    uc_arena_free(&arena);
    return 0;
}

static int test_guitar_pedalboard_project_compiles_and_runs(void) {
    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_project_v2_resolved_t project;
    if (load_resolved_project("projects-v2/guitar-pedalboard.project.v2.yaml", &arena, &project)) {
        uc_arena_free(&arena);
        return 1;
    }

    apg_project_v2_compiled_t compiled;
    if (compile_resolved_project(&project, &arena, &compiled)) {
        uc_arena_free(&arena);
        return 1;
    }

    if (compiled.expanded_unit.params_len != 12u ||
        strcmp(compiled.expanded_unit.params[0].name, "gate1.threshold") != 0)
        return fail("pedalboard project params were not namespaced");
    if (compiled.expanded_unit.nodes_len != 16u || compiled.plan.nodes_len != 16u)
        return fail("pedalboard project did not expand the product unit graphs");

    apg_v2_runtime_t runtime;
    uc_error         err    = {0};
    uc_status        status = apg_v2_runtime_init(&compiled.plan, 8u, 48000.0f, &runtime, &err);
    if (status != UC_OK) {
        fprintf(stderr, "runtime init error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("failed to initialize pedalboard runtime");
    }

    if (!apg_v2_runtime_set_param(&runtime, "blend1.mix", 0.5f))
        return fail("pedalboard runtime did not accept namespaced mix param");

    const float input[4]  = {0.3f, 0.6f, -0.2f, 0.1f};
    float       output[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    if (!apg_v2_runtime_process_mono_ports(&runtime, "input", input, "output", output, 4u)) {
        fprintf(stderr, "runtime error: %s\n", apg_v2_runtime_last_error(&runtime));
        return fail("pedalboard project processing failed");
    }
    if (expect_finite_samples(output, 4u, "pedalboard project"))
        return 1;

    apg_v2_runtime_destroy(&runtime);
    uc_arena_free(&arena);
    return 0;
}

static int expect_compile_error_contains(const char *yaml, const char *label, const char *must_contain) {
    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_project_v2_resolved_t project;
    uc_error                  err    = {0};
    uc_status                 status = apg_project_v2_load_string(yaml, strlen(yaml), &arena, &project.project, &err);
    if (status != UC_OK) {
        fprintf(stderr, "project schema error for %s: %s\n", label, err.msg);
        uc_arena_free(&arena);
        return 1;
    }
    project.units_len = 1u;
    project.units     = uc_arena_alloc(&arena, sizeof(*project.units), sizeof(void *));
    if (!project.units) {
        uc_arena_free(&arena);
        return fail("arena OOM");
    }
    project.units[0].id            = "gain_unit";
    project.units[0].file          = "../units-v2/simple_gain.unit.v2.yaml";
    project.units[0].resolved_path = "units-v2/simple_gain.unit.v2.yaml";
    status = apg_unit_v2_load_file("units-v2/simple_gain.unit.v2.yaml", &arena, &project.units[0].unit, &err);
    if (status != UC_OK) {
        fprintf(stderr, "unit load error for %s: %s\n", label, err.msg);
        uc_arena_free(&arena);
        return 1;
    }

    apg_project_v2_compiled_t compiled;
    status = apg_project_v2_compile(&project, &arena, &compiled, &err);
    uc_arena_free(&arena);
    if (status == UC_OK) {
        fprintf(stderr, "accepted invalid project compile case: %s\n", label);
        return 1;
    }
    if (must_contain && !strstr(err.msg, must_contain)) {
        fprintf(stderr, "project compile error for %s lacked '%s': %s\n", label, must_contain, err.msg);
        return 1;
    }
    return 0;
}

static int test_compile_rejects_unknown_instance_param(void) {
    const char *yaml = "kind: apg.project\n"
                       "schema: apg.project.v2\n"
                       "name: bad-param\n"
                       "version: 2.0.0\n"
                       "units:\n"
                       "  - id: gain_unit\n"
                       "    file: ../units-v2/simple_gain.unit.v2.yaml\n"
                       "chain:\n"
                       "  nodes:\n"
                       "    - id: gain1\n"
                       "      unit: gain_unit\n"
                       "      params:\n"
                       "        missing: 1.0\n"
                       "  routes:\n"
                       "    - from: system.input\n"
                       "      to: gain1.input\n"
                       "    - from: gain1.output\n"
                       "      to: system.output\n"
                       "targets:\n"
                       "  default: desktop_full\n";
    return expect_compile_error_contains(yaml, "unknown instance param", "missing");
}

static int test_compile_rejects_bad_port_route(void) {
    const char *yaml = "kind: apg.project\n"
                       "schema: apg.project.v2\n"
                       "name: bad-port\n"
                       "version: 2.0.0\n"
                       "units:\n"
                       "  - id: gain_unit\n"
                       "    file: ../units-v2/simple_gain.unit.v2.yaml\n"
                       "chain:\n"
                       "  nodes:\n"
                       "    - id: gain1\n"
                       "      unit: gain_unit\n"
                       "  routes:\n"
                       "    - from: system.input\n"
                       "      to: gain1.missing\n"
                       "    - from: gain1.output\n"
                       "      to: system.output\n"
                       "targets:\n"
                       "  default: desktop_full\n";
    return expect_compile_error_contains(yaml, "bad route port", "unknown input port");
}

int main(void) {
    if (test_simple_project_compiles_and_runs())
        return 1;
    if (test_two_instance_project_compiles_and_runs())
        return 1;
    if (test_guitar_pedalboard_project_compiles_and_runs())
        return 1;
    if (test_compile_rejects_unknown_instance_param())
        return 1;
    if (test_compile_rejects_bad_port_route())
        return 1;
    return 0;
}
