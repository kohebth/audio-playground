#include <apgcore/compiler_v2.h>
#include <apgcore/runtime_v2.h>
#include <apgcore/unit_v2.h>

#include <stdint.h>
#include <stdio.h>

static int fail(const char *msg) {
    fprintf(stderr, "FAIL: %s\n", msg);
    return 1;
}

static int
load_and_compile_fixture(const char *path, uc_arena *arena, apg_unit_v2_t *unit, apg_v2_compiled_unit_t *plan) {
    uc_error  err    = {0};
    uc_status status = apg_unit_v2_load_file(path, arena, unit, &err);
    if (status != UC_OK) {
        fprintf(stderr, "load error: %s\n", err.msg);
        return fail("failed to load v2 fixture");
    }

    status = apg_v2_compile_unit(unit, arena, plan, &err);
    if (status != UC_OK) {
        fprintf(stderr, "compile error: %s\n", err.msg);
        return fail("failed to compile v2 fixture");
    }
    return 0;
}

static int test_runtime_init_simple_gain(void) {
    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_unit_v2_t          unit;
    apg_v2_compiled_unit_t plan;
    if (load_and_compile_fixture("units-v2/simple_gain.unit.v2.yaml", &arena, &unit, &plan)) {
        uc_arena_free(&arena);
        return 1;
    }

    apg_v2_runtime_t runtime;
    uc_error         err    = {0};
    uc_status        status = apg_v2_runtime_init(&plan, 16u, 44100.0f, &runtime, &err);
    if (status != UC_OK) {
        fprintf(stderr, "runtime init error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("failed to initialize v2 runtime");
    }

    if (runtime.plan != &plan || runtime.frame_capacity != 16u || runtime.process_info.frames != 16u ||
        runtime.process_info.output_frames != 16u || runtime.process_info.sample_rate != 44100.0f ||
        runtime.process_info.channels != 1u)
        return fail("unexpected runtime process metadata");
    if (runtime.signals_len != unit.signals_len || runtime.signals_len != 3u || !runtime.signal_pool ||
        !runtime.signals)
        return fail("unexpected runtime signal allocation shape");
    for (size_t i = 0; i < runtime.signals_len; i++) {
        if (runtime.signals[i] != &runtime.signal_pool[i * 16u])
            return fail("runtime signal pointer does not point into signal pool");
        for (uint32_t frame = 0; frame < 16u; frame++) {
            if (runtime.signals[i][frame] != 0.0f)
                return fail("runtime signal pool was not zero-initialized");
        }
    }

    if (runtime.params_len != 1u || !runtime.params || runtime.params[0] != 1.0f)
        return fail("unexpected runtime param defaults");
    if (runtime.nodes_len != plan.nodes_len || runtime.nodes_len != 2u || !runtime.nodes)
        return fail("unexpected runtime node allocation shape");
    for (size_t i = 0; i < runtime.nodes_len; i++) {
        apg_v2_runtime_node_t *node = &runtime.nodes[i];
        if (!node->out_storage || !node->in_storage || !node->config_storage || !node->state_storage)
            return fail("runtime node storage is missing");
        if (node->call.out != node->out_storage || node->call.in != node->in_storage ||
            node->call.config != node->config_storage || node->call.state != node->state_storage)
            return fail("runtime atom call does not reference owned storage");
        if (node->call.info != &runtime.process_info)
            return fail("runtime atom call does not reference process info");
    }

    apg_v2_runtime_destroy(&runtime);
    if (runtime.signal_pool || runtime.signals || runtime.params || runtime.nodes || runtime.signals_len != 0u)
        return fail("runtime destroy did not clear owned storage");

    uc_arena_free(&arena);
    return 0;
}

static int test_simple_gain_process_mono(void) {
    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_unit_v2_t          unit;
    apg_v2_compiled_unit_t plan;
    if (load_and_compile_fixture("units-v2/simple_gain.unit.v2.yaml", &arena, &unit, &plan)) {
        uc_arena_free(&arena);
        return 1;
    }

    apg_v2_runtime_t runtime;
    uc_error         err    = {0};
    uc_status        status = apg_v2_runtime_init(&plan, 8u, 48000.0f, &runtime, &err);
    if (status != UC_OK) {
        fprintf(stderr, "runtime init error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("failed to initialize v2 runtime");
    }

    if (!apg_v2_runtime_set_param(&runtime, "gain", 2.0f))
        return fail("failed to set simple_gain param");
    const float input[4]  = {0.25f, -0.5f, 1.5f, -2.0f};
    float       output[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    if (!apg_v2_runtime_process_mono(&runtime, input, output, 4u))
        return fail("simple_gain processing failed");

    const float expected[4] = {0.5f, -1.0f, 3.0f, -4.0f};
    for (size_t i = 0; i < 4u; i++) {
        if (output[i] != expected[i])
            return fail("unexpected simple_gain output sample");
    }
    if (runtime.process_info.frames != 4u || runtime.process_info.output_frames != 4u)
        return fail("runtime process metadata did not track requested frames");
    if (apg_v2_runtime_process_mono(&runtime, input, output, 9u))
        return fail("simple_gain accepted over-capacity frame count");

    apg_v2_runtime_destroy(&runtime);
    uc_arena_free(&arena);
    return 0;
}

static int test_simple_clip_process_generic(void) {
    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_unit_v2_t          unit;
    apg_v2_compiled_unit_t plan;
    if (load_and_compile_fixture("units-v2/simple_clip.unit.v2.yaml", &arena, &unit, &plan)) {
        uc_arena_free(&arena);
        return 1;
    }

    apg_v2_runtime_t runtime;
    uc_error         err    = {0};
    uc_status        status = apg_v2_runtime_init(&plan, 8u, 48000.0f, &runtime, &err);
    if (status != UC_OK) {
        fprintf(stderr, "runtime init error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("failed to initialize v2 runtime");
    }

    if (!apg_v2_runtime_set_param(&runtime, "gain", 2.0f))
        return fail("failed to set simple_clip param");
    float *input  = apg_v2_runtime_find_signal(&runtime, "input");
    float *output = apg_v2_runtime_find_signal(&runtime, "output");
    if (!input || !output)
        return fail("failed to find simple_clip signals");

    input[0] = 0.25f;
    input[1] = -0.5f;
    input[2] = 1.0f;
    input[3] = -2.0f;
    if (!apg_v2_runtime_process(&runtime, 4u))
        return fail("simple_clip generic processing failed");

    const float expected[4] = {0.5f, -0.75f, 0.75f, -0.75f};
    for (size_t i = 0; i < 4u; i++) {
        if (output[i] != expected[i])
            return fail("unexpected simple_clip output sample");
    }
    if (apg_v2_runtime_find_signal(&runtime, "missing"))
        return fail("missing signal lookup unexpectedly succeeded");
    if (apg_v2_runtime_set_param(&runtime, "missing", 1.0f))
        return fail("missing param update unexpectedly succeeded");

    apg_v2_runtime_destroy(&runtime);
    uc_arena_free(&arena);
    return 0;
}

static int test_simple_mix_process_generic(void) {
    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_unit_v2_t          unit;
    apg_v2_compiled_unit_t plan;
    if (load_and_compile_fixture("units-v2/simple_mix.unit.v2.yaml", &arena, &unit, &plan)) {
        uc_arena_free(&arena);
        return 1;
    }

    apg_v2_runtime_t runtime;
    uc_error         err    = {0};
    uc_status        status = apg_v2_runtime_init(&plan, 8u, 48000.0f, &runtime, &err);
    if (status != UC_OK) {
        fprintf(stderr, "runtime init error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("failed to initialize v2 runtime");
    }

    float *a      = apg_v2_runtime_find_signal(&runtime, "a");
    float *b      = apg_v2_runtime_find_signal(&runtime, "b");
    float *output = apg_v2_runtime_find_signal(&runtime, "output");
    if (!a || !b || !output)
        return fail("failed to find simple_mix signals");

    a[0] = 0.25f;
    a[1] = -0.5f;
    a[2] = 1.5f;
    a[3] = -2.0f;
    b[0] = 0.75f;
    b[1] = 0.25f;
    b[2] = -0.5f;
    b[3] = 1.0f;
    if (!apg_v2_runtime_process(&runtime, 4u))
        return fail("simple_mix generic processing failed");

    const float expected[4] = {1.0f, -0.25f, 1.0f, -1.0f};
    for (size_t i = 0; i < 4u; i++) {
        if (output[i] != expected[i])
            return fail("unexpected simple_mix output sample");
    }

    apg_v2_runtime_destroy(&runtime);
    uc_arena_free(&arena);
    return 0;
}

static int test_runtime_init_rejects_zero_capacity(void) {
    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_unit_v2_t          unit;
    apg_v2_compiled_unit_t plan;
    if (load_and_compile_fixture("units-v2/simple_gain.unit.v2.yaml", &arena, &unit, &plan)) {
        uc_arena_free(&arena);
        return 1;
    }

    apg_v2_runtime_t runtime;
    uc_error         err    = {0};
    uc_status        status = apg_v2_runtime_init(&plan, 0u, 48000.0f, &runtime, &err);
    uc_arena_free(&arena);
    if (status == UC_OK)
        return fail("runtime accepted zero frame capacity");
    return 0;
}

int main(void) {
    if (test_runtime_init_simple_gain())
        return 1;
    if (test_simple_gain_process_mono())
        return 1;
    if (test_simple_clip_process_generic())
        return 1;
    if (test_simple_mix_process_generic())
        return 1;
    if (test_runtime_init_rejects_zero_capacity())
        return 1;
    return 0;
}
