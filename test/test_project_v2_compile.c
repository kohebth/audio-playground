#include <apgcore/compiler/project_compiler_v2.h>
#include <apgcore/measure/measure_v2.h>
#include <apgcore/registry/registry_builder_v2.h>
#include <apgcore/runtime/runtime_v2.h>
#include <apgcore/runtime/runtime_v2_internal.h>

#include "test_runtime_v2_harness.h"

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

static int test_empty_project_compiles_and_passes_through(void) {
    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_project_v2_resolved_t project;
    if (load_resolved_project("test/fixtures/projects-v2/empty-passthrough.project.v2.yaml", &arena, &project)) {
        uc_arena_free(&arena);
        return 1;
    }

    apg_project_v2_compiled_t compiled;
    if (compile_resolved_project(&project, &arena, &compiled)) {
        uc_arena_free(&arena);
        return 1;
    }

    if (compiled.expanded_unit.params_len != 0u || compiled.expanded_unit.nodes_len != 0u ||
        compiled.expanded_unit.signals_len != 1u || strcmp(compiled.expanded_unit.signals[0], "input") != 0 ||
        compiled.plan.nodes_len != 0u || compiled.plan.schedule_len != 0u || compiled.plan.instances_len != 0u) {
        uc_arena_free(&arena);
        return fail("empty project did not compile to a zero-node pass-through plan");
    }

    apg_v2_runtime_t runtime;
    uc_error         err    = {0};
    uc_status        status = test_apg_v2_runtime_init_registry(&compiled.plan, 8u, 48000.0f, &arena, &runtime, &err);
    if (status != UC_OK) {
        fprintf(stderr, "empty runtime init error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("failed to initialize empty project runtime");
    }

    const float input[4]  = {0.25f, -0.5f, 1.0f, -1.0f};
    float       output[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    if (!test_runtime_process_mono_ports(&runtime, "input", input, "output", output, 4u)) {
        fprintf(stderr, "empty runtime error: %s\n", apg_v2_measure_last_error(&runtime));
        apg_v2_runtime_destroy(&runtime);
        uc_arena_free(&arena);
        return fail("empty project runtime processing failed");
    }
    if (expect_samples(output, input, 4u, "empty project pass-through")) {
        apg_v2_runtime_destroy(&runtime);
        uc_arena_free(&arena);
        return 1;
    }

    apg_v2_runtime_destroy(&runtime);
    uc_arena_free(&arena);
    return 0;
}

static int test_simple_project_compiles_and_runs(void) {
    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_project_v2_resolved_t project;
    if (load_resolved_project("test/fixtures/projects-v2/simple-gain-board.project.v2.yaml", &arena, &project)) {
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
    if (compiled.plan.instances_len != 1u || compiled.plan.instance_index_by_node_len != 2u ||
        compiled.plan.instance_index_by_node[0] != 0u || compiled.plan.instance_index_by_node[1] != 0u)
        return fail("project compiler did not produce instance metadata");
    if (compiled.plan.instances[0].id_len != 5u || strncmp(compiled.plan.instances[0].id, "gain1", 5u) != 0 ||
        !compiled.plan.instances[0].bypassable || compiled.plan.instances[0].input_signal_index == (size_t)-1u ||
        compiled.plan.instances[0].output_signal_index == (size_t)-1u)
        return fail("project compiler produced unexpected gain1 instance metadata");

    apg_v2_runtime_t runtime;
    uc_error         err    = {0};
    uc_status        status = test_apg_v2_runtime_init_registry(&compiled.plan, 8u, 48000.0f, &arena, &runtime, &err);
    if (status != UC_OK) {
        fprintf(stderr, "runtime init error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("failed to initialize project runtime");
    }

    const float input[3]  = {0.25f, -0.5f, 1.0f};
    float       output[3] = {0.0f, 0.0f, 0.0f};
    if (!test_runtime_process_mono_ports(&runtime, "input", input, "output", output, 3u)) {
        fprintf(stderr, "runtime error: %s\n", apg_v2_measure_last_error(&runtime));
        return fail("project runtime processing failed");
    }
    const float expected_default[3] = {0.5f, -1.0f, 2.0f};
    if (expect_samples(output, expected_default, 3u, "project default gain"))
        return 1;

    if (!test_runtime_set_param_by_name(&runtime, "gain1.gain", 3.0f))
        return fail("project runtime did not accept namespaced param");
    if (test_runtime_set_param_by_name(&runtime, "gain", 3.0f))
        return fail("project runtime accepted unqualified param");
    if (!test_runtime_process_mono_ports(&runtime, "input", input, "output", output, 3u))
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
    if (load_resolved_project("test/fixtures/projects-v2/two-gain-chain.project.v2.yaml", &arena, &project)) {
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
    if (compiled.plan.instances_len != 2u || compiled.plan.instance_index_by_node_len != 4u ||
        compiled.plan.instance_index_by_node[0] != 0u || compiled.plan.instance_index_by_node[1] != 0u ||
        compiled.plan.instance_index_by_node[2] != 1u || compiled.plan.instance_index_by_node[3] != 1u)
        return fail("two-instance project compiler instance map is wrong");
    if (compiled.plan.instances[0].id_len != 5u || strncmp(compiled.plan.instances[0].id, "gain1", 5u) != 0 ||
        compiled.plan.instances[1].id_len != 5u || strncmp(compiled.plan.instances[1].id, "gain2", 5u) != 0)
        return fail("two-instance project compiler instance ids are wrong");

    apg_v2_runtime_t runtime;
    uc_error         err    = {0};
    uc_status        status = test_apg_v2_runtime_init_registry(&compiled.plan, 8u, 48000.0f, &arena, &runtime, &err);
    if (status != UC_OK) {
        fprintf(stderr, "runtime init error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("failed to initialize two-instance project runtime");
    }

    apg_v2_meter_snapshot_t meter;
    if (!apg_v2_measure_get_output_meter(&runtime, "output", 0u, &meter) || meter.valid)
        return fail("two-instance project output meter was not initially empty");
    if (apg_v2_measure_get_output_meter(&runtime, "missing", 0u, &meter))
        return fail("two-instance project accepted missing output meter");

    const float input[2]  = {0.25f, -0.5f};
    float       output[2] = {0.0f, 0.0f};
    if (!test_runtime_process_mono_ports(&runtime, "input", input, "output", output, 2u))
        return fail("two-instance project processing failed");
    const float expected_default[2] = {1.5f, -3.0f};
    if (expect_samples(output, expected_default, 2u, "two-instance project default"))
        return 1;
    if (!apg_v2_measure_get_input_meter(&runtime, "input", 0u, &meter) ||
        expect_meter_near(&meter, 0.5f, 0.3952847f, 2u, "two-instance project input"))
        return 1;
    if (!apg_v2_measure_get_output_meter(&runtime, "output", 0u, &meter) ||
        expect_meter_near(&meter, 3.0f, 2.3717082f, 2u, "two-instance project output"))
        return 1;

    if (!test_runtime_set_instance_bypass_by_name(&runtime, "gain1", true))
        return fail("two-instance runtime did not accept first instance bypass");
    if (!test_runtime_process_mono_ports(&runtime, "input", input, "output", output, 2u))
        return fail("two-instance project processing with bypass failed");
    const float expected_bypassed[2] = {0.75f, -1.5f};
    if (expect_samples(output, expected_bypassed, 2u, "two-instance project bypassed"))
        return 1;
    if (!apg_v2_measure_get_output_meter(&runtime, "output", 0u, &meter) ||
        expect_meter_near(&meter, 1.5f, 1.1858541f, 2u, "two-instance project bypassed output"))
        return 1;

    if (!apg_v2_runtime_set_project_mute(&runtime, true))
        return fail("two-instance runtime did not accept project mute");
    if (!test_runtime_process_mono_ports(&runtime, "input", input, "output", output, 2u))
        return fail("two-instance project processing with mute failed");
    const float expected_muted[2] = {0.0f, 0.0f};
    if (expect_samples(output, expected_muted, 2u, "two-instance project muted"))
        return 1;
    if (!apg_v2_measure_get_output_meter(&runtime, "output", 0u, &meter) ||
        expect_meter_near(&meter, 0.0f, 0.0f, 2u, "two-instance project muted output"))
        return 1;

    if (!apg_v2_runtime_set_project_mute(&runtime, false) ||
        !test_runtime_set_instance_bypass_by_name(&runtime, "gain1", false))
        return fail("two-instance runtime did not disable project controls");
    if (!test_runtime_process_mono_ports(&runtime, "input", input, "output", output, 2u))
        return fail("two-instance project processing after disabling controls failed");
    if (expect_samples(output, expected_default, 2u, "two-instance project restored"))
        return 1;

    if (!test_runtime_set_param_by_name(&runtime, "gain2.gain", 4.0f))
        return fail("two-instance runtime did not accept second instance param");
    if (!test_runtime_process_mono_ports(&runtime, "input", input, "output", output, 2u))
        return fail("two-instance project processing after update failed");
    const float expected_updated[2] = {1.5020833f, -3.0041666f};
    if (expect_samples_near(output, expected_updated, 2u, 0.00001f, "two-instance project updated"))
        return 1;

    apg_v2_runtime_destroy(&runtime);
    uc_arena_free(&arena);
    return 0;
}

static const apg_v2_compiled_instance_t *find_instance(const apg_v2_compiled_unit_t *plan, const char *id) {
    size_t id_len = strlen(id);
    for (size_t i = 0; plan && i < plan->instances_len; i++) {
        if (plan->instances[i].id_len == id_len && strncmp(plan->instances[i].id, id, id_len) == 0)
            return &plan->instances[i];
    }
    return NULL;
}

static int test_parallel_project_compiles_runs_and_locks_routing_bypass(void) {
    uc_arena arena;
    if (uc_arena_init(&arena, 2 * 1024 * 1024) != 0)
        return fail("arena init failed");
    apg_project_v2_resolved_t project;
    if (load_resolved_project("test/fixtures/projects-v2/parallel-gain.project.v2.yaml", &arena, &project)) {
        uc_arena_free(&arena);
        return 1;
    }
    apg_project_v2_compiled_t compiled;
    if (compile_resolved_project(&project, &arena, &compiled)) {
        uc_arena_free(&arena);
        return 1;
    }

    const apg_v2_compiled_instance_t *panner = find_instance(&compiled.plan, "parallel_pan");
    const apg_v2_compiled_instance_t *mixer  = find_instance(&compiled.plan, "parallel_mix");
    const apg_v2_compiled_instance_t *boost  = find_instance(&compiled.plan, "boost");
    if (!panner || !mixer || !boost || panner->bypassable || mixer->bypassable || !boost->bypassable) {
        uc_arena_free(&arena);
        return fail("routing instance bypass metadata is wrong");
    }

    apg_v2_runtime_t runtime;
    uc_error         err    = {0};
    uc_status        status = test_apg_v2_runtime_init_registry(&compiled.plan, 8u, 48000.0f, &arena, &runtime, &err);
    if (status != UC_OK) {
        fprintf(stderr, "parallel runtime init error: %s\n", err.msg);
        uc_arena_free(&arena);
        return 1;
    }
    const float input[3]    = {0.25f, -0.5f, 1.0f};
    const float expected[3] = {0.375f, -0.75f, 1.5f};
    float       output[3]   = {0.0f, 0.0f, 0.0f};
    if (!test_runtime_process_mono_ports(&runtime, "input", input, "output", output, 3u) ||
        expect_samples_near(output, expected, 3u, 0.00002f, "parallel mix output")) {
        apg_v2_runtime_destroy(&runtime);
        uc_arena_free(&arena);
        return 1;
    }
    apg_v2_runtime_destroy(&runtime);
    uc_arena_free(&arena);
    return 0;
}

static int test_guitar_pedalboard_project_compiles_and_runs(void) {
    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_project_v2_resolved_t project;
    if (load_resolved_project("test/fixtures/projects-v2/guitar-pedalboard.project.v2.yaml", &arena, &project)) {
        uc_arena_free(&arena);
        return 1;
    }

    apg_project_v2_compiled_t compiled;
    if (compile_resolved_project(&project, &arena, &compiled)) {
        uc_arena_free(&arena);
        return 1;
    }

    if (compiled.expanded_unit.params_len != 27u ||
        strcmp(compiled.expanded_unit.params[0].name, "gate1.threshold") != 0 ||
        strcmp(compiled.expanded_unit.params[1].name, "gate1.attack") != 0 ||
        strcmp(compiled.expanded_unit.params[2].name, "gate1.release") != 0 ||
        strcmp(compiled.expanded_unit.params[3].name, "phaser1.rate") != 0 ||
        strcmp(compiled.expanded_unit.params[7].name, "phaser1.mix") != 0 ||
        strcmp(compiled.expanded_unit.params[11].name, "tone1.gain") != 0 ||
        strcmp(compiled.expanded_unit.params[15].name, "tone1.presence") != 0 ||
        strcmp(compiled.expanded_unit.params[16].name, "tone1.volume") != 0 ||
        strcmp(compiled.expanded_unit.params[19].name, "chorus1.rate") != 0 ||
        strcmp(compiled.expanded_unit.params[23].name, "delay1.feedback") != 0)
        return fail("pedalboard project params were not namespaced");
    if (compiled.expanded_unit.nodes_len != 52u || compiled.plan.nodes_len != 52u)
        return fail("pedalboard project did not expand the product unit graphs");
    if (compiled.plan.instances_len != 8u || compiled.plan.instances[0].id_len != 5u ||
        strncmp(compiled.plan.instances[0].id, "gate1", 5u) != 0 ||
        strncmp(compiled.plan.instances[7].id, "reverb1", 7u) != 0)
        return fail("pedalboard compiler instance metadata is wrong");
    for (size_t i = 0; i < compiled.plan.instances_len; i++) {
        if (compiled.plan.instances[i].bypassable && (compiled.plan.instances[i].input_signal_index == (size_t)-1u ||
                                                      compiled.plan.instances[i].output_signal_index == (size_t)-1u))
            return fail("pedalboard bypassable instance is missing io metadata");
    }

    apg_v2_runtime_t runtime;
    uc_error         err    = {0};
    uc_status        status = test_apg_v2_runtime_init_registry(&compiled.plan, 8u, 48000.0f, &arena, &runtime, &err);
    if (status != UC_OK) {
        fprintf(stderr, "runtime init error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("failed to initialize pedalboard runtime");
    }

    if (!test_runtime_set_param_by_name(&runtime, "delay1.feedback", 0.5f))
        return fail("pedalboard runtime did not accept namespaced delay feedback param");

    const float input[4]  = {0.3f, 0.6f, -0.2f, 0.1f};
    float       output[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    if (!test_runtime_process_mono_ports(&runtime, "input", input, "output", output, 4u)) {
        fprintf(stderr, "runtime error: %s\n", apg_v2_measure_last_error(&runtime));
        return fail("pedalboard project processing failed");
    }
    if (expect_finite_samples(output, 4u, "pedalboard project"))
        return 1;

    apg_v2_runtime_destroy(&runtime);
    uc_arena_free(&arena);
    return 0;
}

static int test_guitar_project_state_buffer_table_layout(void) {
    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_project_v2_resolved_t project;
    if (load_resolved_project("test/fixtures/projects-v2/guitar-pedalboard.project.v2.yaml", &arena, &project)) {
        uc_arena_free(&arena);
        return 1;
    }

    apg_project_v2_compiled_t compiled;
    if (compile_resolved_project(&project, &arena, &compiled)) {
        uc_arena_free(&arena);
        return 1;
    }

    uc_arena registry_arena;
    if (uc_arena_init(&registry_arena, 8192) != 0) {
        uc_arena_free(&arena);
        return fail("registry arena init failed");
    }

    apg_v2_registry_t           registry;
    uc_error                    err             = {0};
    const apg_prepare_context_t prepare_context = {
        .maximum_frames = 8u,
        .sample_rate    = 48000.0f,
    };
    uc_status status =
        apg_v2_registry_build_with_growth(&compiled.plan, &prepare_context, &registry_arena, &registry, &err);
    if (status != UC_OK) {
        fprintf(stderr, "registry build error: %s\n", err.msg);
        uc_arena_free(&registry_arena);
        uc_arena_free(&arena);
        return fail("failed to build guitar pedalboard registry");
    }

    if (registry.state_buffers_len == 0u)
        return fail("guitar pedalboard registry should contain stateful nodes");

    size_t expected_table_offset = 0u;
    for (size_t i = 0; i < registry.nodes_len; i++) {
        const apg_v2_registry_node_layout_t *layout = &registry.node_layouts[i];
        if (layout->state_buffer_table_offset != expected_table_offset)
            return fail("state-buffer table offset was not compacted across nodes");
        for (size_t buffer_index = 0; buffer_index < layout->state_buffers_len; buffer_index++) {
            if (layout->state_buffer_samples_by_index[buffer_index] == 0u)
                return fail("state-buffer metadata is missing sample size");
            size_t sample_offset = layout->state_buffer_sample_offsets_by_index[buffer_index];
            size_t sample_count  = layout->state_buffer_samples_by_index[buffer_index];
            if (sample_offset + sample_count > registry.state_buffer_samples)
                return fail("state-buffer sample offset is outside registry pool");
            expected_table_offset++;
        }
    }
    if (expected_table_offset != registry.state_buffers_len)
        return fail("state-buffer table is not fully consumed by node layouts");

    apg_v2_runtime_t runtime;
    status = apg_v2_runtime_init_from_registry(&registry, &runtime, &err);
    if (status != UC_OK) {
        fprintf(stderr, "runtime init error: %s\n", err.msg);
        uc_arena_free(&registry_arena);
        uc_arena_free(&arena);
        return fail("failed to initialize runtime from guitar pedalboard registry");
    }

    for (size_t i = 0; i < runtime.nodes_len; i++) {
        const apg_v2_runtime_node_t         *node   = &runtime.nodes[i];
        const apg_v2_registry_node_layout_t *layout = &registry.node_layouts[i];
        for (size_t buffer_index = 0; buffer_index < layout->state_buffers_len; buffer_index++) {
            size_t       registry_slot = layout->state_buffer_table_offset + buffer_index;
            const float *expected_ptr =
                runtime.state_buffer_pool + layout->state_buffer_sample_offsets_by_index[buffer_index];
            if (runtime.state_buffer_ptrs[registry_slot] != expected_ptr)
                return fail("state-buffer runtime pointer table does not match registry offsets");
            if (runtime.state_buffer_sample_counts[registry_slot] !=
                layout->state_buffer_samples_by_index[buffer_index])
                return fail("state-buffer sample-count table does not match registry metadata");
            if (node->state_buffers[buffer_index] != expected_ptr ||
                node->state_buffer_samples[buffer_index] != layout->state_buffer_samples_by_index[buffer_index])
                return fail("node state-buffer tables are not projected from registry");
        }
    }

    apg_v2_runtime_destroy(&runtime);
    uc_arena_free(&registry_arena);
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
    project.units[0].resolved_path = "test/fixtures/units-v2/simple_gain.unit.v2.yaml";
    status =
        apg_unit_v2_load_file("test/fixtures/units-v2/simple_gain.unit.v2.yaml", &arena, &project.units[0].unit, &err);
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
    if (test_empty_project_compiles_and_passes_through())
        return 1;
    if (test_simple_project_compiles_and_runs())
        return 1;
    if (test_two_instance_project_compiles_and_runs())
        return 1;
    if (test_parallel_project_compiles_runs_and_locks_routing_bypass())
        return 1;
    if (test_guitar_pedalboard_project_compiles_and_runs())
        return 1;
    if (test_guitar_project_state_buffer_table_layout())
        return 1;
    if (test_compile_rejects_unknown_instance_param())
        return 1;
    if (test_compile_rejects_bad_port_route())
        return 1;
    return 0;
}
