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

int main(void) {
    if (test_runtime_image_layout())
        return 1;
    return test_runtime_image_control_targets();
}
