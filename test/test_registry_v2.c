#include <apgcore/compiler/compiler_v2.h>
#include <apgcore/measure/measure_v2.h>
#include <apgcore/registry/registry_builder_v2.h>
#include <apgcore/runtime/runtime_v2.h>
#include <apgcore/runtime/runtime_v2_internal.h>
#include <apgcore/validator/unit_v2.h>
#include <atom/dsp_types.h>

#include "test_runtime_v2_harness.h"

#include <stdio.h>
#include <string.h>

static int fail(const char *msg) {
    fprintf(stderr, "FAIL: %s\n", msg);
    return 1;
}

static float *runtime_signal_by_name_for_test(apg_v2_runtime_t *runtime, const char *name) {
    if (!runtime || !name)
        return NULL;
    for (size_t i = 0; i < runtime->signals_len; i++) {
        if (runtime->signal_names[i] && strcmp(runtime->signal_names[i], name) == 0)
            return apg_v2_runtime_signal_buffer_at_mut(runtime, i);
    }
    return NULL;
}

static int load_compile_fixture(const char *path, uc_arena *arena, apg_unit_v2_t *unit, apg_v2_compiled_unit_t *plan) {
    uc_error  err    = {0};
    uc_status status = apg_unit_v2_load_file(path, arena, unit, &err);
    if (status != UC_OK) {
        fprintf(stderr, "load error: %s\n", err.msg);
        return fail("failed to load fixture");
    }

    status = apg_v2_compile_unit(unit, arena, plan, &err);
    if (status != UC_OK) {
        fprintf(stderr, "compile error: %s\n", err.msg);
        return fail("failed to compile fixture");
    }
    return 0;
}

static int load_compile_string(const char *yaml, uc_arena *arena, apg_unit_v2_t *unit, apg_v2_compiled_unit_t *plan) {
    uc_error  err    = {0};
    uc_status status = apg_unit_v2_load_string(yaml, strlen(yaml), arena, unit, &err);
    if (status != UC_OK) {
        fprintf(stderr, "load error: %s\n", err.msg);
        return fail("failed to load unit string");
    }

    status = apg_v2_compile_unit(unit, arena, plan, &err);
    if (status != UC_OK) {
        fprintf(stderr, "compile error: %s\n", err.msg);
        return fail("failed to compile unit string");
    }
    return 0;
}

static int test_registry_spectral_context(void) {
    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_unit_v2_t          unit;
    apg_v2_compiled_unit_t plan;
    if (load_compile_fixture("test/fixtures/units-v2/wasm_unsupported_freq.unit.v2.yaml", &arena, &unit, &plan)) {
        uc_arena_free(&arena);
        return 1;
    }
    if (plan.nodes_len != 1u || !plan.nodes[0].has_spectral_info || plan.nodes[0].spectral_info.fft_size != 256u ||
        plan.nodes[0].spectral_info.bin_count != 129u || plan.nodes[0].spectral_info.hop_size != 256u) {
        uc_arena_free(&arena);
        return fail("compiler did not derive spectral context");
    }

    uc_arena          registry_arena;
    apg_v2_registry_t registry;
    uc_error          err = {0};
    uc_status status      = apg_v2_registry_build_with_growth(&plan, 64u, 48000.0f, &registry_arena, &registry, &err);
    if (status != UC_E_RANGE || strstr(err.msg, "frame capacity") == NULL) {
        if (status == UC_OK)
            uc_arena_free(&registry_arena);
        uc_arena_free(&arena);
        return fail("registry accepted undersized spectral signal buffers");
    }

    err    = (uc_error){0};
    status = apg_v2_registry_build_with_growth(&plan, 256u, 48000.0f, &registry_arena, &registry, &err);
    if (status != UC_OK) {
        fprintf(stderr, "registry error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("failed to build spectral registry");
    }
    if (registry.nodes_len != 1u || !registry.node_layouts[0].has_spectral_info ||
        !apg_spectral_info_valid(&registry.node_layouts[0].spectral_info)) {
        uc_arena_free(&registry_arena);
        uc_arena_free(&arena);
        return fail("registry did not preserve spectral context");
    }

    apg_v2_runtime_t runtime;
    status = apg_v2_runtime_init_from_registry(&registry, &runtime, &err);
    if (status != UC_OK) {
        fprintf(stderr, "runtime error: %s\n", err.msg);
        uc_arena_free(&registry_arena);
        uc_arena_free(&arena);
        return fail("failed to initialize spectral runtime");
    }
    if (runtime.nodes_len != 1u || runtime.nodes[0].call.spectral_info != &registry.node_layouts[0].spectral_info) {
        apg_v2_runtime_destroy(&runtime);
        uc_arena_free(&registry_arena);
        uc_arena_free(&arena);
        return fail("runtime call does not reference registry-owned spectral context");
    }

    apg_v2_runtime_destroy(&runtime);
    uc_arena_free(&registry_arena);
    uc_arena_free(&arena);
    return 0;
}

static int test_registry_spectral_overlap_buffers(void) {
    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");
    apg_unit_v2_t          unit;
    apg_v2_compiled_unit_t plan;
    if (load_compile_fixture("test/fixtures/units-v2/spectral_overlap.unit.v2.yaml", &arena, &unit, &plan)) {
        uc_arena_free(&arena);
        return 1;
    }
    uc_arena          registry_arena;
    apg_v2_registry_t registry;
    uc_error          err = {0};
    uc_status status      = apg_v2_registry_build_with_growth(&plan, 256u, 48000.0f, &registry_arena, &registry, &err);
    if (status != UC_OK) {
        fprintf(stderr, "registry error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("failed to build spectral overlap registry");
    }
    if (registry.nodes_len != 2u || registry.state_buffers_len != 2u || registry.state_buffer_samples != 512u) {
        uc_arena_free(&registry_arena);
        uc_arena_free(&arena);
        return fail("spectral overlap registry buffer total mismatch");
    }
    for (size_t i = 0; i < registry.nodes_len; i++) {
        if (registry.node_layouts[i].state_buffers_len != 1u ||
            registry.node_layouts[i].state_buffer_samples_by_index[0] != 256u) {
            uc_arena_free(&registry_arena);
            uc_arena_free(&arena);
            return fail("spectral overlap buffer was not sized from fft_size");
        }
    }
    uc_arena_free(&registry_arena);
    uc_arena_free(&arena);
    return 0;
}

static int test_registry_layout(void) {
    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_unit_v2_t          unit;
    apg_v2_compiled_unit_t plan;
    if (load_compile_fixture("test/fixtures/units-v2/simple_gain.unit.v2.yaml", &arena, &unit, &plan)) {
        uc_arena_free(&arena);
        return 1;
    }

    uc_arena registry_arena;
    if (uc_arena_init(&registry_arena, 4096) != 0) {
        uc_arena_free(&arena);
        return fail("registry arena init failed");
    }

    apg_v2_registry_t registry;
    uc_error          err    = {0};
    uc_status         status = apg_v2_registry_build(&plan, 16u, 44100.0f, &registry_arena, &registry, &err);
    if (status != UC_OK) {
        fprintf(stderr, "registry error: %s\n", err.msg);
        uc_arena_free(&registry_arena);
        uc_arena_free(&arena);
        return fail("failed to build registry");
    }

    if (registry.frame_capacity != 16u || registry.sample_rate != 44100.0f)
        return fail("unexpected registry identity");
    if (registry.signals_len != 3u || registry.signal_samples != 48u || registry.params_len != 1u ||
        !registry.param_defaults || registry.param_defaults[0] != 1.0f || !registry.param_smoothing_frames ||
        registry.param_smoothing_frames[0] != 441u)
        return fail("unexpected registry signal or param layout");
    if (!registry.signal_names || registry.signal_names == unit.signals ||
        strcmp(registry.signal_names[0], "input") != 0 || !registry.param_names ||
        strcmp(registry.param_names[0], "gain") != 0)
        return fail("registry did not expose lookup names");
    if (registry.input_meters_len != 1u || registry.output_meters_len != 1u || registry.nodes_len != 2u ||
        registry.schedule_len != 2u || registry.state_buffers_len != 0u || registry.state_buffer_samples != 0u)
        return fail("unexpected registry execution layout");
    if (registry.schedule == plan.schedule || registry.schedule[0] != plan.schedule[0] ||
        registry.schedule[1] != plan.schedule[1])
        return fail("registry did not copy compiled schedule");
    if (registry.atom_storage_bytes == 0u)
        return fail("registry did not reserve atom storage");
    if (!registry.node_layouts || registry.node_layouts[0].out_size == 0u || registry.node_layouts[0].in_size == 0u ||
        registry.node_layouts[0].config_size == 0u || registry.node_layouts[0].state_size == 0u)
        return fail("unexpected registry node layout");
    if (registry.node_layouts[0].out_offset >= registry.atom_storage_bytes ||
        registry.node_layouts[1].state_offset >= registry.atom_storage_bytes)
        return fail("registry atom storage offsets are out of range");
    if (registry.node_layouts[0].state_buffer_samples_by_index)
        return fail("stateless node should not have state buffer sample layout");
    if (registry.node_layouts[0].config_refreshes_len != 1u || registry.node_layouts[1].config_refreshes_len != 0u)
        return fail("unexpected registry config refresh layout");
    if (strcmp(registry.node_layouts[0].config_refreshes[0].key, "value") != 0 ||
        registry.node_layouts[0].config_refreshes[0].kind != APG_BIND_PARAM ||
        registry.node_layouts[0].config_refreshes[0].param_index != 0u)
        return fail("registry did not copy scalar refresh metadata");
    if (registry.node_layouts[0].signal_bindings_len != 1u || !registry.node_layouts[0].signal_bindings ||
        registry.node_layouts[1].signal_bindings_len != 3u || !registry.node_layouts[1].signal_bindings)
        return fail("unexpected registry signal binding layout");

    apg_v2_runtime_t runtime;
    status = apg_v2_runtime_init_from_registry(&registry, &runtime, &err);
    if (status != UC_OK) {
        fprintf(stderr, "runtime init error: %s\n", err.msg);
        uc_arena_free(&registry_arena);
        uc_arena_free(&arena);
        return fail("failed to initialize runtime from registry");
    }

    if (runtime.frame_capacity != registry.frame_capacity || runtime.process_info.sample_rate != registry.sample_rate ||
        runtime.signals_len != registry.signals_len || runtime.params_len != registry.params_len ||
        runtime.params[0] != registry.param_defaults[0] || runtime.input_meters_len != registry.input_meters_len ||
        runtime.output_meters_len != registry.output_meters_len || runtime.nodes_len != registry.nodes_len ||
        runtime.schedule != registry.schedule || runtime.schedule_len != registry.schedule_len)
        return fail("runtime did not adopt registry layout");
    if (runtime.nodes[0].thunk != registry.node_layouts[0].thunk ||
        strcmp(runtime.nodes[0].atom_name, registry.node_layouts[0].atom_name) != 0 ||
        strcmp(runtime.nodes[1].node_id, registry.node_layouts[1].node_id) != 0)
        return fail("runtime nodes did not adopt registry execution metadata");
    if (runtime.param_smoothing_frames != registry.param_smoothing_frames)
        return fail("runtime did not adopt registry param smoothing layout");
    if (runtime.signal_names != registry.signal_names || runtime.param_names != registry.param_names)
        return fail("runtime did not adopt registry lookup names");
    if (!runtime.atom_storage_pool || runtime.atom_storage_bytes != registry.atom_storage_bytes)
        return fail("runtime did not allocate registry atom storage pool");
    const char *pool = (const char *)runtime.atom_storage_pool;
    if ((const char *)runtime.nodes[0].out_storage < pool ||
        (const char *)runtime.nodes[0].out_storage >= pool + runtime.atom_storage_bytes ||
        (const char *)runtime.nodes[1].state_storage < pool ||
        (const char *)runtime.nodes[1].state_storage >= pool + runtime.atom_storage_bytes)
        return fail("runtime node storage does not point into atom storage pool");
    if (runtime.nodes[0].config_refreshes_len != registry.node_layouts[0].config_refreshes_len ||
        runtime.nodes[1].config_refreshes_len != registry.node_layouts[1].config_refreshes_len)
        return fail("runtime did not adopt config refresh plan");
    if (runtime.nodes[0].signal_bindings != registry.node_layouts[0].signal_bindings ||
        runtime.nodes[0].signal_bindings_len != registry.node_layouts[0].signal_bindings_len ||
        runtime.nodes[1].signal_bindings != registry.node_layouts[1].signal_bindings ||
        runtime.nodes[1].signal_bindings_len != registry.node_layouts[1].signal_bindings_len)
        return fail("runtime did not adopt signal binding plan");

    if (!runtime_signal_by_name_for_test(&runtime, "input") || !test_runtime_set_param_by_name(&runtime, "gain", 1.0f))
        return fail("registry lookup metadata failed");
    if (!apg_v2_runtime_reset(&runtime))
        return fail("registry reset failed");

    float input[4]  = {0.25f, -0.5f, 0.75f, -1.0f};
    float output[4] = {0};
    if (!test_runtime_process_mono_ports(&runtime, "input", input, "output", output, 4u))
        return fail("registry process failed");
    for (size_t i = 0; i < 4u; i++) {
        if (output[i] != input[i])
            return fail("registry output mismatch");
    }

    apg_v2_runtime_destroy(&runtime);
    uc_arena_free(&registry_arena);
    uc_arena_free(&arena);
    return 0;
}

static int test_registry_state_buffer_samples(void) {
    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_unit_v2_t          unit;
    apg_v2_compiled_unit_t plan;
    if (load_compile_fixture("test/fixtures/units-v2/delay_line_state.unit.v2.yaml", &arena, &unit, &plan)) {
        uc_arena_free(&arena);
        return 1;
    }

    uc_arena registry_arena;
    if (uc_arena_init(&registry_arena, 4096) != 0) {
        uc_arena_free(&arena);
        return fail("registry arena init failed");
    }

    apg_v2_registry_t registry;
    uc_error          err    = {0};
    uc_status         status = apg_v2_registry_build(&plan, 64u, 48000.0f, &registry_arena, &registry, &err);
    if (status != UC_OK) {
        fprintf(stderr, "registry error: %s\n", err.msg);
        uc_arena_free(&registry_arena);
        uc_arena_free(&arena);
        return fail("failed to build stateful registry");
    }

    size_t stateful_layouts = 0u;
    for (size_t i = 0; i < registry.nodes_len; i++) {
        const apg_v2_registry_node_layout_t *layout = &registry.node_layouts[i];
        if (layout->state_buffers_len == 0u)
            continue;
        stateful_layouts++;
        if (!layout->state_buffer_samples_by_index || !layout->state_buffer_sample_offsets_by_index)
            return fail("stateful node is missing state buffer sample layout");
        for (size_t j = 0; j < layout->state_buffers_len; j++) {
            if (layout->state_buffer_samples_by_index[j] == 0u)
                return fail("state buffer sample layout did not preserve capacity");
            if (layout->state_buffer_sample_offsets_by_index[j] >= registry.state_buffer_samples)
                return fail("state buffer sample offset is out of range");
        }
    }
    if (stateful_layouts == 0u || registry.state_buffers_len == 0u || registry.state_buffer_samples == 0u)
        return fail("expected stateful registry layout");
    if (registry.state_buffers_len != 1u || registry.state_buffer_samples != 33u ||
        registry.node_layouts[0].state_buffer_samples_by_index[0] != 33u)
        return fail("delay_line buffer was not sized from parameter maximum");

    apg_v2_runtime_t runtime;
    status = apg_v2_runtime_init_from_registry(&registry, &runtime, &err);
    if (status != UC_OK) {
        fprintf(stderr, "runtime init error: %s\n", err.msg);
        uc_arena_free(&registry_arena);
        uc_arena_free(&arena);
        return fail("failed to initialize stateful runtime");
    }
    if (!runtime.state_buffer_pool || runtime.state_buffer_samples != registry.state_buffer_samples)
        return fail("runtime did not allocate registry state buffer pool");
    const delay_line_state_t *delay_state = (const delay_line_state_t *)runtime.nodes[0].state_storage;
    if (!delay_state || delay_state->buffer_len != 33u)
        return fail("runtime did not bind delay_line buffer_len");
    for (size_t i = 0; i < runtime.nodes_len; i++) {
        for (size_t j = 0; j < runtime.nodes[i].state_buffers_len; j++) {
            const float *buffer = runtime.nodes[i].state_buffers[j];
            if (buffer < runtime.state_buffer_pool ||
                buffer >= runtime.state_buffer_pool + runtime.state_buffer_samples)
                return fail("runtime state buffer does not point into state buffer pool");
        }
    }

    apg_v2_runtime_destroy(&runtime);
    uc_arena_free(&registry_arena);
    uc_arena_free(&arena);
    return 0;
}

static int test_registry_control_targets(void) {
    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_unit_v2_t          unit;
    apg_v2_compiled_unit_t plan;
    if (load_compile_fixture("test/fixtures/units-v2/control_gain.unit.v2.yaml", &arena, &unit, &plan)) {
        uc_arena_free(&arena);
        return 1;
    }

    uc_arena registry_arena;
    if (uc_arena_init(&registry_arena, 4096) != 0) {
        uc_arena_free(&arena);
        return fail("registry arena init failed");
    }

    apg_v2_registry_t registry;
    uc_error          err    = {0};
    uc_status         status = apg_v2_registry_build(&plan, 8u, 48000.0f, &registry_arena, &registry, &err);
    if (status != UC_OK) {
        fprintf(stderr, "registry error: %s\n", err.msg);
        uc_arena_free(&registry_arena);
        uc_arena_free(&arena);
        return fail("failed to build control registry");
    }
    if (registry.control_targets_len != 1u || !registry.control_targets ||
        strcmp(registry.control_targets[0].port_name, "amount") != 0 ||
        strcmp(registry.control_targets[0].param_name, "gain") != 0 || registry.control_targets[0].param_index != 0u)
        return fail("unexpected registry control target");

    unit.input_ports[0].name = "mutated_amount";
    unit.params[0].name      = "mutated_gain";

    apg_v2_runtime_t runtime;
    status = apg_v2_runtime_init_from_registry(&registry, &runtime, &err);
    if (status != UC_OK) {
        fprintf(stderr, "runtime init error: %s\n", err.msg);
        uc_arena_free(&registry_arena);
        uc_arena_free(&arena);
        return fail("failed to initialize control runtime from registry");
    }
    if (runtime.control_targets_len != 1u || !runtime.control_targets ||
        strcmp(runtime.control_targets[0].port_name, "amount") != 0)
        return fail("runtime did not copy control target registry");
    if (!test_runtime_set_control_port_by_name(&runtime, "amount", 2.0f) || runtime.params[0] != 2.0f)
        return fail("runtime control target did not update param");

    apg_v2_runtime_destroy(&runtime);
    uc_arena_free(&registry_arena);
    uc_arena_free(&arena);
    return 0;
}

static int test_registry_signal_array_pool(void) {
    const char *yaml = "kind: apg.unit\n"
                       "schema: apg.unit.v2\n"
                       "name: matrix_registry\n"
                       "version: 2.0.0\n"
                       "params: {}\n"
                       "ports:\n"
                       "  inputs:\n"
                       "    - name: a\n"
                       "      type: audio\n"
                       "      channels: 1\n"
                       "    - name: b\n"
                       "      type: audio\n"
                       "      channels: 1\n"
                       "  outputs:\n"
                       "    - name: sum\n"
                       "      type: audio\n"
                       "      channels: 1\n"
                       "    - name: diff\n"
                       "      type: audio\n"
                       "      channels: 1\n"
                       "graph:\n"
                       "  signals:\n"
                       "    - a\n"
                       "    - b\n"
                       "    - sum\n"
                       "    - diff\n"
                       "  nodes:\n"
                       "    - id: matrix\n"
                       "      atom: mix_matrix\n"
                       "      in:\n"
                       "        signals:\n"
                       "          - a\n"
                       "          - b\n"
                       "      out:\n"
                       "        signals:\n"
                       "          - sum\n"
                       "          - diff\n"
                       "      config:\n"
                       "        coefficients:\n"
                       "          row0: { c0: 0.5, c1: 0.5 }\n"
                       "          row1: { c0: 1.0, c1: -1.0 }\n"
                       "compatibility:\n"
                       "  desktop_full: true\n";

    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_unit_v2_t          unit;
    apg_v2_compiled_unit_t plan;
    if (load_compile_string(yaml, &arena, &unit, &plan)) {
        uc_arena_free(&arena);
        return 1;
    }

    uc_arena registry_arena;
    if (uc_arena_init(&registry_arena, 4096) != 0) {
        uc_arena_free(&arena);
        return fail("registry arena init failed");
    }

    apg_v2_registry_t registry;
    uc_error          err    = {0};
    uc_status         status = apg_v2_registry_build(&plan, 8u, 48000.0f, &registry_arena, &registry, &err);
    if (status != UC_OK) {
        fprintf(stderr, "registry error: %s\n", err.msg);
        uc_arena_free(&registry_arena);
        uc_arena_free(&arena);
        return fail("failed to build signal-array registry");
    }
    if (registry.nodes_len != 1u || registry.signal_array_pointer_slots != 4u ||
        registry.node_layouts[0].signal_array_pointer_slots != 4u ||
        registry.node_layouts[0].signal_bindings_len != 2u || !registry.node_layouts[0].signal_bindings)
        return fail("unexpected signal-array pointer pool layout");
    if (!registry.node_layouts[0].signal_bindings[0].signal_array_indices ||
        !registry.node_layouts[0].signal_bindings[1].signal_array_indices ||
        registry.node_layouts[0].signal_bindings[0].signal_array_indices[0] != 2u ||
        registry.node_layouts[0].signal_bindings[1].signal_array_indices[1] != 1u)
        return fail("registry did not copy signal-array binding indexes");

    apg_v2_runtime_t runtime;
    status = apg_v2_runtime_init_from_registry(&registry, &runtime, &err);
    if (status != UC_OK) {
        fprintf(stderr, "runtime init error: %s\n", err.msg);
        uc_arena_free(&registry_arena);
        uc_arena_free(&arena);
        return fail("failed to initialize signal-array runtime");
    }
    if (runtime.signal_array_pool_len != 4u || runtime.signal_array_pool != runtime.nodes[0].signal_array_pool ||
        runtime.nodes[0].signal_array_pool_len != 4u || runtime.nodes[0].signal_array_pool_used != 4u ||
        runtime.nodes[0].signal_bindings != registry.node_layouts[0].signal_bindings ||
        runtime.nodes[0].signal_bindings_len != registry.node_layouts[0].signal_bindings_len)
        return fail("runtime did not consume registry signal-array pool");
    if (!registry.node_layouts[0].mix_matrix_row_pointers || !registry.node_layouts[0].mix_matrix_coefficients ||
        registry.node_layouts[0].mix_matrix_coefficients_len != 4u ||
        registry.node_layouts[0].mix_matrix_num_out != 2u || registry.node_layouts[0].mix_matrix_num_in != 2u ||
        registry.node_layouts[0].mix_matrix_coefficients[0] != 0.5f ||
        registry.node_layouts[0].mix_matrix_coefficients[1] != 0.5f ||
        registry.node_layouts[0].mix_matrix_coefficients[2] != 1.0f ||
        registry.node_layouts[0].mix_matrix_coefficients[3] != -1.0f)
        return fail("unexpected mix_matrix registry matrix layout");

    const mix_matrix_params_t *matrix_params = (const mix_matrix_params_t *)runtime.nodes[0].config_storage;
    if (!matrix_params->coefficients || matrix_params->num_in != 2 || matrix_params->num_out != 2)
        return fail("runtime did not consume mix_matrix matrix layout");
    if (matrix_params->coefficients[0] != registry.node_layouts[0].mix_matrix_row_pointers[0] ||
        matrix_params->coefficients[1] != registry.node_layouts[0].mix_matrix_row_pointers[1])
        return fail("runtime mix_matrix matrix rows did not point to registry rows");
    if (matrix_params->coefficients[0][0] != 0.5f || matrix_params->coefficients[0][1] != 0.5f ||
        matrix_params->coefficients[1][0] != 1.0f || matrix_params->coefficients[1][1] != -1.0f)
        return fail("runtime mix_matrix coefficients mismatch");

    apg_v2_runtime_destroy(&runtime);
    uc_arena_free(&registry_arena);
    uc_arena_free(&arena);
    return 0;
}

static int test_registry_scalar_input_refresh(void) {
    const char *yaml = "kind: apg.unit\n"
                       "schema: apg.unit.v2\n"
                       "name: tap_registry\n"
                       "version: 2.0.0\n"
                       "params:\n"
                       "  tap:\n"
                       "    type: int\n"
                       "    default: 2\n"
                       "    min: 0\n"
                       "    max: 8\n"
                       "ports:\n"
                       "  inputs:\n"
                       "    - name: input\n"
                       "      type: audio\n"
                       "      channels: 1\n"
                       "  outputs:\n"
                       "    - name: output\n"
                       "      type: audio\n"
                       "      channels: 1\n"
                       "graph:\n"
                       "  signals:\n"
                       "    - input\n"
                       "    - output\n"
                       "  nodes:\n"
                       "    - id: tap\n"
                       "      atom: delay_tap_feedback\n"
                       "      in:\n"
                       "        buffer: input\n"
                       "        tap_position: ${params.tap}\n"
                       "      out:\n"
                       "        signal: output\n"
                       "      config:\n"
                       "        coefficient: 0.5\n"
                       "compatibility:\n"
                       "  desktop_full: true\n";

    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_unit_v2_t          unit;
    apg_v2_compiled_unit_t plan;
    if (load_compile_string(yaml, &arena, &unit, &plan)) {
        uc_arena_free(&arena);
        return 1;
    }

    uc_arena registry_arena;
    if (uc_arena_init(&registry_arena, 4096) != 0) {
        uc_arena_free(&arena);
        return fail("registry arena init failed");
    }

    apg_v2_registry_t registry;
    uc_error          err    = {0};
    uc_status         status = apg_v2_registry_build(&plan, 8u, 48000.0f, &registry_arena, &registry, &err);
    if (status != UC_OK) {
        fprintf(stderr, "registry error: %s\n", err.msg);
        uc_arena_free(&registry_arena);
        uc_arena_free(&arena);
        return fail("failed to build scalar-input registry");
    }
    if (registry.nodes_len != 1u || registry.node_layouts[0].input_refreshes_len != 1u ||
        registry.node_layouts[0].config_refreshes_len != 1u)
        return fail("unexpected scalar refresh registry layout");

    apg_v2_runtime_t runtime;
    status = apg_v2_runtime_init_from_registry(&registry, &runtime, &err);
    if (status != UC_OK) {
        fprintf(stderr, "runtime init error: %s\n", err.msg);
        uc_arena_free(&registry_arena);
        uc_arena_free(&arena);
        return fail("failed to initialize scalar-input runtime");
    }
    if (runtime.nodes[0].input_refreshes_len != 1u || runtime.nodes[0].config_refreshes_len != 1u)
        return fail("runtime did not adopt scalar refresh plans");

    apg_v2_runtime_destroy(&runtime);
    uc_arena_free(&registry_arena);
    uc_arena_free(&arena);
    return 0;
}

static int test_registry_uses_compiler_instance_metadata(void) {
    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_unit_v2_t          unit;
    apg_v2_compiled_unit_t plan;
    if (load_compile_fixture("test/fixtures/units-v2/simple_gain.unit.v2.yaml", &arena, &unit, &plan)) {
        uc_arena_free(&arena);
        return 1;
    }

    plan.nodes[1].id = "mutated_node_id";

    uc_arena registry_arena;
    if (uc_arena_init(&registry_arena, 4096) != 0) {
        uc_arena_free(&arena);
        return fail("registry arena init failed");
    }

    apg_v2_registry_t registry;
    uc_error          err    = {0};
    uc_status         status = apg_v2_registry_build(&plan, 16u, 44100.0f, &registry_arena, &registry, &err);
    if (status != UC_OK) {
        fprintf(stderr, "registry error: %s\n", err.msg);
        uc_arena_free(&registry_arena);
        uc_arena_free(&arena);
        return fail("failed to build registry from compiler instance metadata");
    }

    if (registry.bypassed_instances_len != 1u || !registry.bypass_instances ||
        strncmp(registry.bypass_instances[0].instance_id, "apply_gain", registry.bypass_instances[0].instance_id_len) !=
            0 ||
        registry.bypass_instances[0].input_index != 0u || registry.bypass_instances[0].output_index != 1u)
        return fail("registry did not use compiler instance bypass metadata");
    if (!registry.bypass_index_by_node || registry.bypass_index_by_node[1] != 0u)
        return fail("registry did not use compiler node-to-instance metadata");
    plan.instances[0].id     = "mutated_instance";
    plan.instances[0].id_len = strlen(plan.instances[0].id);
    if (strncmp(registry.bypass_instances[0].instance_id, "apply_gain", registry.bypass_instances[0].instance_id_len) !=
        0)
        return fail("registry bypass metadata borrowed compiler instance strings");

    uc_arena_free(&registry_arena);
    uc_arena_free(&arena);
    return 0;
}

static int test_runtime_init_from_registry_ignores_plan_mutation(void) {
    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_unit_v2_t          unit;
    apg_v2_compiled_unit_t plan;
    if (load_compile_fixture("test/fixtures/units-v2/simple_gain.unit.v2.yaml", &arena, &unit, &plan)) {
        uc_arena_free(&arena);
        return 1;
    }

    uc_arena registry_arena;
    if (uc_arena_init(&registry_arena, 4096) != 0) {
        uc_arena_free(&arena);
        return fail("registry arena init failed");
    }

    apg_v2_registry_t registry;
    uc_error          err    = {0};
    uc_status         status = apg_v2_registry_build(&plan, 16u, 44100.0f, &registry_arena, &registry, &err);
    if (status != UC_OK) {
        fprintf(stderr, "registry error: %s\n", err.msg);
        uc_arena_free(&registry_arena);
        uc_arena_free(&arena);
        return fail("failed to build registry");
    }

    unit.signals[0]             = "mutated_input_signal";
    unit.signals[1]             = "mutated_output_signal";
    unit.params[0].name         = "mutated_gain";
    unit.input_ports[0].name    = "mutated_input_port";
    unit.output_ports[0].name   = "mutated_output_port";
    plan.nodes[0].id            = "mutated_node_id";
    plan.nodes[0].atom_name     = "mutated_atom_name";
    plan.nodes[0].config[0].key = "mutated_value_key";
    plan.nodes[1].in[0].key     = "mutated_signal_key";

    plan.nodes_len            = 0;
    plan.schedule_len         = 0;
    plan.signal_producers     = NULL;
    plan.signal_producers_len = 0;
    if (plan.nodes) {
        plan.nodes[0].atom = NULL;
    }

    apg_v2_runtime_t runtime;
    err    = (uc_error){0};
    status = apg_v2_runtime_init_from_registry(&registry, &runtime, &err);
    if (status != UC_OK) {
        fprintf(stderr, "runtime init error after plan mutation: %s\n", err.msg);
        uc_arena_free(&registry_arena);
        uc_arena_free(&arena);
        return fail("runtime should initialize from built registry regardless of plan mutation");
    }
    if (strcmp(runtime.signal_names[0], "input") != 0 || strcmp(runtime.param_names[0], "gain") != 0 ||
        strcmp(runtime.nodes[0].node_id, "gain_value") != 0 ||
        strcmp(runtime.nodes[0].atom_name, "generation_dc") != 0 ||
        strcmp(runtime.nodes[0].config_refreshes[0].key, "value") != 0)
        return fail("runtime lookup metadata borrowed mutated compiler strings");
    if (!test_runtime_set_param_by_name(&runtime, "gain", 1.0f))
        return fail("runtime param lookup failed after plan string mutation");

    float input[4]  = {0.25f, -0.5f, 1.0f, -1.0f};
    float output[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    if (!test_runtime_process_mono_ports(&runtime, "input", input, "output", output, 4u)) {
        fprintf(stderr, "runtime process error: %s\n", apg_v2_measure_last_error(&runtime));
        apg_v2_runtime_destroy(&runtime);
        uc_arena_free(&registry_arena);
        uc_arena_free(&arena);
        return fail("runtime should still process from registry schedule after plan mutation");
    }

    for (size_t i = 0; i < 4u; i++) {
        if (output[i] != input[i]) {
            apg_v2_runtime_destroy(&runtime);
            uc_arena_free(&registry_arena);
            uc_arena_free(&arena);
            return fail("runtime output changed after plan mutation");
        }
    }

    apg_v2_runtime_destroy(&runtime);
    uc_arena_free(&registry_arena);
    uc_arena_free(&arena);
    return 0;
}

static int test_registry_builds_from_compiled_atom_layout_without_raw_atom(void) {
    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_unit_v2_t          unit;
    apg_v2_compiled_unit_t plan;
    if (load_compile_fixture("test/fixtures/units-v2/simple_gain.unit.v2.yaml", &arena, &unit, &plan)) {
        uc_arena_free(&arena);
        return 1;
    }

    for (size_t i = 0; i < plan.nodes_len; i++)
        plan.nodes[i].atom = NULL;

    uc_arena registry_arena;
    if (uc_arena_init(&registry_arena, 4096) != 0) {
        uc_arena_free(&arena);
        return fail("registry arena init failed");
    }

    apg_v2_registry_t registry;
    uc_error          err    = {0};
    uc_status         status = apg_v2_registry_build(&plan, 16u, 44100.0f, &registry_arena, &registry, &err);
    if (status != UC_OK) {
        fprintf(stderr, "registry error without raw atom pointer: %s\n", err.msg);
        uc_arena_free(&registry_arena);
        uc_arena_free(&arena);
        return fail("registry should build from compiled atom layout");
    }

    if (registry.nodes_len != 2u || !registry.node_layouts || !registry.node_layouts[0].thunk ||
        strcmp(registry.node_layouts[0].atom_name, "generation_dc") != 0 ||
        strcmp(registry.node_layouts[1].atom_name, "amplitude_multiply") != 0 ||
        registry.node_layouts[0].config_refreshes_len != 1u || registry.node_layouts[1].signal_bindings_len != 3u)
        return fail("registry did not consume compiled atom layout facts");

    uc_arena_free(&registry_arena);
    uc_arena_free(&arena);
    return 0;
}

static int test_runtime_create_owned_lifecycle(void) {
    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_unit_v2_t          unit;
    apg_v2_compiled_unit_t plan;
    if (load_compile_fixture("test/fixtures/units-v2/simple_gain.unit.v2.yaml", &arena, &unit, &plan)) {
        uc_arena_free(&arena);
        return 1;
    }

    uc_arena registry_arena;
    if (uc_arena_init(&registry_arena, 4096) != 0) {
        uc_arena_free(&arena);
        return fail("registry arena init failed");
    }

    apg_v2_registry_t registry;
    uc_error          err    = {0};
    uc_status         status = apg_v2_registry_build(&plan, 16u, 48000.0f, &registry_arena, &registry, &err);
    if (status != UC_OK) {
        fprintf(stderr, "registry error: %s\n", err.msg);
        uc_arena_free(&registry_arena);
        uc_arena_free(&arena);
        return fail("failed to build registry");
    }

    apg_v2_runtime_t *runtime = NULL;
    err                       = (uc_error){0};
    status                    = apg_v2_runtime_create_from_registry(&registry, &runtime, &err);
    if (status != UC_OK || !runtime) {
        fprintf(stderr, "runtime create error: %s\n", err.msg);
        uc_arena_free(&registry_arena);
        uc_arena_free(&arena);
        return fail("failed to create owned runtime");
    }
    if (runtime->frame_capacity != 16u || runtime->process_info.frames != 16u || runtime->output_meters_len != 1u) {
        apg_v2_runtime_destroy_owned(&runtime);
        uc_arena_free(&registry_arena);
        uc_arena_free(&arena);
        return fail("runtime from registry not initialized as expected");
    }

    const float input[4]  = {0.25f, -0.5f, 1.0f, -1.0f};
    float       output[4] = {0.0f};
    if (!test_runtime_process_mono_ports(runtime, "input", input, "output", output, 4u)) {
        fprintf(stderr, "runtime process failed: %s\n", apg_v2_measure_last_error(runtime));
        apg_v2_runtime_destroy_owned(&runtime);
        uc_arena_free(&registry_arena);
        uc_arena_free(&arena);
        return fail("runtime owned object failed to process");
    }

    for (size_t i = 0; i < 4u; i++) {
        if (output[i] != input[i]) {
            apg_v2_runtime_destroy_owned(&runtime);
            uc_arena_free(&registry_arena);
            uc_arena_free(&arena);
            return fail("owned runtime output changed unexpectedly");
        }
    }

    apg_v2_runtime_destroy_owned(&runtime);
    apg_v2_runtime_destroy_owned(NULL);
    if (runtime != NULL)
        return fail("owned destroy did not null runtime pointer");

    uc_arena_free(&registry_arena);
    uc_arena_free(&arena);
    return 0;
}

int main(void) {
    if (test_registry_spectral_context())
        return 1;
    if (test_registry_spectral_overlap_buffers())
        return 1;
    if (test_registry_layout())
        return 1;
    if (test_registry_state_buffer_samples())
        return 1;
    if (test_registry_signal_array_pool())
        return 1;
    if (test_registry_scalar_input_refresh())
        return 1;
    if (test_registry_control_targets())
        return 1;
    if (test_registry_uses_compiler_instance_metadata())
        return 1;
    if (test_registry_builds_from_compiled_atom_layout_without_raw_atom())
        return 1;
    if (test_runtime_create_owned_lifecycle())
        return 1;
    return test_runtime_init_from_registry_ignores_plan_mutation();
}
