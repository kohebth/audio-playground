#include <apgcore/compiler_v2.h>
#include <apgcore/runtime_image_v2.h>
#include <apgcore/runtime_v2.h>
#include <apgcore/unit_v2.h>

#include <stdio.h>
#include <string.h>

static int fail(const char *msg) {
    fprintf(stderr, "FAIL: %s\n", msg);
    return 1;
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

static int test_runtime_image_layout(void) {
    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_unit_v2_t          unit;
    apg_v2_compiled_unit_t plan;
    if (load_compile_fixture("units-v2/simple_gain.unit.v2.yaml", &arena, &unit, &plan)) {
        uc_arena_free(&arena);
        return 1;
    }

    uc_arena image_arena;
    if (uc_arena_init(&image_arena, 4096) != 0) {
        uc_arena_free(&arena);
        return fail("image arena init failed");
    }

    apg_v2_runtime_image_t image;
    uc_error               err    = {0};
    uc_status              status = apg_v2_runtime_image_build(&plan, 16u, 44100.0f, &image_arena, &image, &err);
    if (status != UC_OK) {
        fprintf(stderr, "runtime image error: %s\n", err.msg);
        uc_arena_free(&image_arena);
        uc_arena_free(&arena);
        return fail("failed to build runtime image");
    }

    if (image.plan != &plan || image.frame_capacity != 16u || image.sample_rate != 44100.0f)
        return fail("unexpected runtime image identity");
    if (image.signals_len != 3u || image.signal_samples != 48u || image.params_len != 1u || !image.param_defaults ||
        image.param_defaults[0] != 1.0f)
        return fail("unexpected runtime image signal or param layout");
    if (image.input_meters_len != 1u || image.output_meters_len != 1u || image.nodes_len != 2u ||
        image.schedule_len != 2u || image.state_buffers_len != 0u || image.state_buffer_samples != 0u)
        return fail("unexpected runtime image execution layout");
    if (image.atom_storage_bytes == 0u)
        return fail("runtime image did not reserve atom storage");
    if (!image.node_layouts || image.node_layouts[0].out_size == 0u || image.node_layouts[0].in_size == 0u ||
        image.node_layouts[0].config_size == 0u || image.node_layouts[0].state_size == 0u)
        return fail("unexpected runtime image node layout");
    if (image.node_layouts[0].out_offset >= image.atom_storage_bytes ||
        image.node_layouts[1].state_offset >= image.atom_storage_bytes)
        return fail("runtime image atom storage offsets are out of range");
    if (image.node_layouts[0].state_buffer_samples_by_index)
        return fail("stateless node should not have state buffer sample layout");
    if (image.node_layouts[0].config_refreshes_len != 1u || image.node_layouts[1].config_refreshes_len != 0u)
        return fail("unexpected runtime image config refresh layout");

    apg_v2_runtime_t runtime;
    status = apg_v2_runtime_init_from_image(&image, &runtime, &err);
    if (status != UC_OK) {
        fprintf(stderr, "runtime init error: %s\n", err.msg);
        uc_arena_free(&image_arena);
        uc_arena_free(&arena);
        return fail("failed to initialize runtime from image");
    }

    if (runtime.plan != &plan || runtime.frame_capacity != image.frame_capacity ||
        runtime.process_info.sample_rate != image.sample_rate || runtime.signals_len != image.signals_len ||
        runtime.params_len != image.params_len || runtime.params[0] != image.param_defaults[0] ||
        runtime.input_meters_len != image.input_meters_len || runtime.output_meters_len != image.output_meters_len ||
        runtime.nodes_len != image.nodes_len)
        return fail("runtime did not adopt image layout");
    if (!runtime.atom_storage_pool || runtime.atom_storage_bytes != image.atom_storage_bytes)
        return fail("runtime did not allocate image atom storage pool");
    const char *pool = (const char *)runtime.atom_storage_pool;
    if ((const char *)runtime.nodes[0].out_storage < pool ||
        (const char *)runtime.nodes[0].out_storage >= pool + runtime.atom_storage_bytes ||
        (const char *)runtime.nodes[1].state_storage < pool ||
        (const char *)runtime.nodes[1].state_storage >= pool + runtime.atom_storage_bytes)
        return fail("runtime node storage does not point into atom storage pool");
    if (runtime.nodes[0].config_refreshes_len != image.node_layouts[0].config_refreshes_len ||
        runtime.nodes[1].config_refreshes_len != image.node_layouts[1].config_refreshes_len)
        return fail("runtime did not adopt config refresh plan");

    float input[4]  = {0.25f, -0.5f, 0.75f, -1.0f};
    float output[4] = {0};
    if (!apg_v2_runtime_process_mono_ports(&runtime, "input", input, "output", output, 4u))
        return fail("runtime image process failed");
    for (size_t i = 0; i < 4u; i++) {
        if (output[i] != input[i])
            return fail("runtime image output mismatch");
    }

    apg_v2_runtime_destroy(&runtime);
    uc_arena_free(&image_arena);
    uc_arena_free(&arena);
    return 0;
}

static int test_runtime_image_state_buffer_samples(void) {
    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_unit_v2_t          unit;
    apg_v2_compiled_unit_t plan;
    if (load_compile_fixture("units-v2/delay_line_state.unit.v2.yaml", &arena, &unit, &plan)) {
        uc_arena_free(&arena);
        return 1;
    }

    uc_arena image_arena;
    if (uc_arena_init(&image_arena, 4096) != 0) {
        uc_arena_free(&arena);
        return fail("image arena init failed");
    }

    apg_v2_runtime_image_t image;
    uc_error               err    = {0};
    uc_status              status = apg_v2_runtime_image_build(&plan, 64u, 48000.0f, &image_arena, &image, &err);
    if (status != UC_OK) {
        fprintf(stderr, "runtime image error: %s\n", err.msg);
        uc_arena_free(&image_arena);
        uc_arena_free(&arena);
        return fail("failed to build stateful runtime image");
    }

    size_t stateful_layouts = 0u;
    for (size_t i = 0; i < image.nodes_len; i++) {
        const apg_v2_runtime_node_layout_t *layout = &image.node_layouts[i];
        if (layout->state_buffers_len == 0u)
            continue;
        stateful_layouts++;
        if (!layout->state_buffer_samples_by_index)
            return fail("stateful node is missing state buffer sample layout");
        for (size_t j = 0; j < layout->state_buffers_len; j++) {
            if (layout->state_buffer_samples_by_index[j] == 0u)
                return fail("state buffer sample layout did not preserve capacity");
        }
    }
    if (stateful_layouts == 0u || image.state_buffers_len == 0u || image.state_buffer_samples == 0u)
        return fail("expected stateful runtime image layout");

    uc_arena_free(&image_arena);
    uc_arena_free(&arena);
    return 0;
}

static int test_runtime_image_control_targets(void) {
    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_unit_v2_t          unit;
    apg_v2_compiled_unit_t plan;
    if (load_compile_fixture("units-v2/control_gain.unit.v2.yaml", &arena, &unit, &plan)) {
        uc_arena_free(&arena);
        return 1;
    }

    uc_arena image_arena;
    if (uc_arena_init(&image_arena, 4096) != 0) {
        uc_arena_free(&arena);
        return fail("image arena init failed");
    }

    apg_v2_runtime_image_t image;
    uc_error               err    = {0};
    uc_status              status = apg_v2_runtime_image_build(&plan, 8u, 48000.0f, &image_arena, &image, &err);
    if (status != UC_OK) {
        fprintf(stderr, "runtime image error: %s\n", err.msg);
        uc_arena_free(&image_arena);
        uc_arena_free(&arena);
        return fail("failed to build control runtime image");
    }
    if (image.control_targets_len != 1u || !image.control_targets ||
        strcmp(image.control_targets[0].port_name, "amount") != 0 ||
        strcmp(image.control_targets[0].param_name, "gain") != 0 || image.control_targets[0].param_index != 0u)
        return fail("unexpected runtime image control target");

    apg_v2_runtime_t runtime;
    status = apg_v2_runtime_init_from_image(&image, &runtime, &err);
    if (status != UC_OK) {
        fprintf(stderr, "runtime init error: %s\n", err.msg);
        uc_arena_free(&image_arena);
        uc_arena_free(&arena);
        return fail("failed to initialize control runtime from image");
    }
    if (runtime.control_targets_len != 1u || !runtime.control_targets ||
        strcmp(runtime.control_targets[0].port_name, "amount") != 0)
        return fail("runtime did not copy control target image");
    if (!apg_v2_runtime_set_control_port(&runtime, "amount", 2.0f) || runtime.params[0] != 2.0f)
        return fail("runtime control target did not update param");

    apg_v2_runtime_destroy(&runtime);
    uc_arena_free(&image_arena);
    uc_arena_free(&arena);
    return 0;
}

static int test_runtime_image_signal_array_pool(void) {
    const char *yaml = "kind: apg.unit\n"
                       "schema: apg.unit.v2\n"
                       "name: matrix_image\n"
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

    uc_arena image_arena;
    if (uc_arena_init(&image_arena, 4096) != 0) {
        uc_arena_free(&arena);
        return fail("image arena init failed");
    }

    apg_v2_runtime_image_t image;
    uc_error               err    = {0};
    uc_status              status = apg_v2_runtime_image_build(&plan, 8u, 48000.0f, &image_arena, &image, &err);
    if (status != UC_OK) {
        fprintf(stderr, "runtime image error: %s\n", err.msg);
        uc_arena_free(&image_arena);
        uc_arena_free(&arena);
        return fail("failed to build signal-array runtime image");
    }
    if (image.nodes_len != 1u || image.node_layouts[0].signal_array_pointer_slots != 4u)
        return fail("unexpected signal-array pointer pool layout");

    apg_v2_runtime_t runtime;
    status = apg_v2_runtime_init_from_image(&image, &runtime, &err);
    if (status != UC_OK) {
        fprintf(stderr, "runtime init error: %s\n", err.msg);
        uc_arena_free(&image_arena);
        uc_arena_free(&arena);
        return fail("failed to initialize signal-array runtime");
    }
    if (runtime.nodes[0].signal_array_pool_len != 4u || runtime.nodes[0].signal_array_pool_used != 4u)
        return fail("runtime did not consume image signal-array pool");

    apg_v2_runtime_destroy(&runtime);
    uc_arena_free(&image_arena);
    uc_arena_free(&arena);
    return 0;
}

static int test_runtime_image_scalar_input_refresh(void) {
    const char *yaml = "kind: apg.unit\n"
                       "schema: apg.unit.v2\n"
                       "name: tap_image\n"
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

    uc_arena image_arena;
    if (uc_arena_init(&image_arena, 4096) != 0) {
        uc_arena_free(&arena);
        return fail("image arena init failed");
    }

    apg_v2_runtime_image_t image;
    uc_error               err    = {0};
    uc_status              status = apg_v2_runtime_image_build(&plan, 8u, 48000.0f, &image_arena, &image, &err);
    if (status != UC_OK) {
        fprintf(stderr, "runtime image error: %s\n", err.msg);
        uc_arena_free(&image_arena);
        uc_arena_free(&arena);
        return fail("failed to build scalar-input runtime image");
    }
    if (image.nodes_len != 1u || image.node_layouts[0].input_refreshes_len != 1u ||
        image.node_layouts[0].config_refreshes_len != 1u)
        return fail("unexpected scalar refresh image layout");

    apg_v2_runtime_t runtime;
    status = apg_v2_runtime_init_from_image(&image, &runtime, &err);
    if (status != UC_OK) {
        fprintf(stderr, "runtime init error: %s\n", err.msg);
        uc_arena_free(&image_arena);
        uc_arena_free(&arena);
        return fail("failed to initialize scalar-input runtime");
    }
    if (runtime.nodes[0].input_refreshes_len != 1u || runtime.nodes[0].config_refreshes_len != 1u)
        return fail("runtime did not adopt scalar refresh plans");

    apg_v2_runtime_destroy(&runtime);
    uc_arena_free(&image_arena);
    uc_arena_free(&arena);
    return 0;
}

int main(void) {
    if (test_runtime_image_layout())
        return 1;
    if (test_runtime_image_state_buffer_samples())
        return 1;
    if (test_runtime_image_signal_array_pool())
        return 1;
    if (test_runtime_image_scalar_input_refresh())
        return 1;
    return test_runtime_image_control_targets();
}
