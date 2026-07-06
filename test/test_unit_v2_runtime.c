#include <apgcore/compiler/compiler_v2.h>
#include <apgcore/host/host_v2.h>
#include <apgcore/measure/measure_v2.h>
#include <apgcore/runtime/runtime_v2.h>
#include <apgcore/runtime/runtime_v2_internal.h>
#include <apgcore/validator/unit_v2.h>
#include <atom/dsp_types.h>

#include "test_runtime_v2_harness.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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

static int
load_and_compile_string(const char *yaml, uc_arena *arena, apg_unit_v2_t *unit, apg_v2_compiled_unit_t *plan) {
    uc_error  err    = {0};
    uc_status status = apg_unit_v2_load_string(yaml, strlen(yaml), arena, unit, &err);
    if (status != UC_OK) {
        fprintf(stderr, "load error: %s\n", err.msg);
        return fail("failed to load inline v2 fixture");
    }

    status = apg_v2_compile_unit(unit, arena, plan, &err);
    if (status != UC_OK) {
        fprintf(stderr, "compile error: %s\n", err.msg);
        return fail("failed to compile inline v2 fixture");
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

static int expect_near(float actual, float expected, float tolerance, const char *label) {
    float diff = actual > expected ? actual - expected : expected - actual;
    if (diff > tolerance) {
        fprintf(stderr, "unexpected %s: got %f expected %f\n", label, actual, expected);
        return 1;
    }
    return 0;
}

static int expect_finite_samples(const float *actual, size_t frames, const char *label) {
    for (size_t i = 0; i < frames; i++) {
        if (!isfinite(actual[i])) {
            fprintf(stderr, "non-finite %s sample at %zu: %f\n", label, i, actual[i]);
            return 1;
        }
    }
    return 0;
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

static float *
runtime_port_channel_for_test(apg_v2_runtime_t *runtime, const char *port_name, size_t channel_index, bool output) {
    size_t port_index   = 0u;
    size_t signal_index = 0u;
    bool   ok           = output ? test_runtime_output_audio_port_index_by_name(runtime, port_name, &port_index)
                                 : test_runtime_input_audio_port_index_by_name(runtime, port_name, &port_index);
    if (!ok)
        return NULL;
    ok = output
             ? apg_v2_runtime_output_port_channel_signal_index(runtime, port_index, channel_index, &signal_index, NULL)
             : apg_v2_runtime_input_port_channel_signal_index(runtime, port_index, channel_index, &signal_index, NULL);
    return ok ? apg_v2_runtime_signal_buffer_at_mut(runtime, signal_index) : NULL;
}

static float *runtime_input_port_signal_for_test(apg_v2_runtime_t *runtime, const char *port_name) {
    return runtime_port_channel_for_test(runtime, port_name, 0u, false);
}

static float *runtime_output_port_signal_for_test(apg_v2_runtime_t *runtime, const char *port_name) {
    return runtime_port_channel_for_test(runtime, port_name, 0u, true);
}

static int test_runtime_init_simple_gain(void) {
    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_unit_v2_t          unit;
    apg_v2_compiled_unit_t plan;
    if (load_and_compile_fixture("test/fixtures/units-v2/simple_gain.unit.v2.yaml", &arena, &unit, &plan)) {
        uc_arena_free(&arena);
        return 1;
    }

    apg_v2_runtime_t runtime;
    uc_error         err    = {0};
    uc_status        status = test_apg_v2_runtime_init_registry(&plan, 16u, 44100.0f, &arena, &runtime, &err);
    if (status != UC_OK) {
        fprintf(stderr, "runtime init error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("failed to initialize v2 runtime");
    }

    if (runtime.frame_capacity != 16u || runtime.process_info.frames != 16u ||
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

    if (runtime.params_len != 1u || !runtime.params || !runtime.param_targets ||
        !runtime.param_smoothing_remaining_frames || runtime.params[0] != 1.0f || runtime.param_targets[0] != 1.0f ||
        runtime.param_smoothing_remaining_frames[0] != 0u)
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
    if (runtime.signal_pool || runtime.signals || runtime.params || runtime.param_targets ||
        runtime.param_smoothing_remaining_frames || runtime.bypassed_instances || runtime.nodes ||
        runtime.signals_len != 0u)
        return fail("runtime destroy did not clear owned storage");

    uc_arena_free(&arena);
    return 0;
}

static int test_v2_host_file_bridge_processes_simple_gain(void) {
    apg_v2_host_unit_t *host = NULL;
    uc_error            err  = {0};
    uc_status           status =
        apg_v2_host_load_file("test/fixtures/units-v2/simple_gain.unit.v2.yaml", 8u, 48000.0f, &host, &err);
    if (status != UC_OK) {
        fprintf(stderr, "host load error: %s\n", err.msg);
        return fail("failed to load v2 host fixture");
    }

    if (!apg_v2_host_set_param(host, "gain", 2.5f)) {
        apg_v2_host_destroy(host);
        return fail("failed to set v2 host param");
    }

    const float input[3]  = {0.25f, -0.5f, 1.0f};
    float       output[3] = {0.0f, 0.0f, 0.0f};
    if (!apg_v2_host_process_mono_ports(host, "input", input, "output", output, 3u)) {
        apg_v2_host_destroy(host);
        return fail("v2 host mono processing failed");
    }
    const float expected[3] = {0.625f, -1.25f, 2.5f};
    if (expect_samples(output, expected, 3u, "v2 host simple_gain")) {
        apg_v2_host_destroy(host);
        return 1;
    }

    apg_v2_host_destroy(host);
    return 0;
}

static int test_runtime_config_error_names_node_atom_and_binding(void) {
    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_unit_v2_t          unit;
    apg_v2_compiled_unit_t plan;
    if (load_and_compile_fixture("test/fixtures/units-v2/simple_gain.unit.v2.yaml", &arena, &unit, &plan)) {
        uc_arena_free(&arena);
        return 1;
    }

    plan.nodes[0].config[0].key = "missing_value";
    apg_v2_runtime_t runtime;
    uc_error         err    = {0};
    uc_status        status = test_apg_v2_runtime_init_registry(&plan, 8u, 48000.0f, &arena, &runtime, &err);
    if (status == UC_OK) {
        apg_v2_runtime_destroy(&runtime);
        uc_arena_free(&arena);
        return fail("registry accepted missing config field metadata");
    }
    if (!strstr(err.msg, "gain_value") || !strstr(err.msg, "generation_dc") || !strstr(err.msg, "config binding key") ||
        !strstr(err.msg, "missing_value"))
        return fail("registry config error did not include node, atom, and binding context");

    uc_arena_free(&arena);
    return 0;
}

static int test_simple_gain_process_mono(void) {
    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_unit_v2_t          unit;
    apg_v2_compiled_unit_t plan;
    if (load_and_compile_fixture("test/fixtures/units-v2/simple_gain.unit.v2.yaml", &arena, &unit, &plan)) {
        uc_arena_free(&arena);
        return 1;
    }

    apg_v2_runtime_t runtime;
    uc_error         err    = {0};
    uc_status        status = test_apg_v2_runtime_init_registry(&plan, 8u, 48000.0f, &arena, &runtime, &err);
    if (status != UC_OK) {
        fprintf(stderr, "runtime init error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("failed to initialize v2 runtime");
    }

    if (!test_runtime_set_param_by_name(&runtime, "gain", 2.0f))
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
    const char *last_error = apg_v2_measure_last_error(&runtime);
    if (!last_error || !strstr(last_error, "capacity"))
        return fail("simple_gain over-capacity failure did not expose a useful error");

    apg_v2_runtime_destroy(&runtime);
    uc_arena_free(&arena);
    return 0;
}

static int test_runtime_param_smoothing_advances_at_block_boundaries(void) {
    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_unit_v2_t          unit;
    apg_v2_compiled_unit_t plan;
    if (load_and_compile_fixture("test/fixtures/units-v2/simple_gain.unit.v2.yaml", &arena, &unit, &plan)) {
        uc_arena_free(&arena);
        return 1;
    }

    apg_v2_runtime_t runtime;
    uc_error         err    = {0};
    uc_status        status = test_apg_v2_runtime_init_registry(&plan, 512u, 48000.0f, &arena, &runtime, &err);
    if (status != UC_OK) {
        fprintf(stderr, "runtime init error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("failed to initialize smoothing runtime");
    }

    float input[512];
    float output[512];
    for (size_t i = 0; i < 512u; i++) {
        input[i]  = 1.0f;
        output[i] = 0.0f;
    }

    if (!test_runtime_set_param_by_name(&runtime, "gain", 2.0f))
        return fail("failed to set initial smoothed param");
    if (!apg_v2_runtime_process_mono(&runtime, input, output, 4u))
        return fail("initial immediate smoothing process failed");
    for (size_t i = 0; i < 4u; i++) {
        if (expect_near(output[i], 2.0f, 0.0001f, "initial immediate smoothing sample"))
            return 1;
    }

    if (!apg_v2_runtime_reset(&runtime))
        return fail("smoothing runtime reset failed");
    if (!apg_v2_runtime_process_mono(&runtime, input, output, 4u))
        return fail("default smoothing process failed");
    for (size_t i = 0; i < 4u; i++) {
        if (expect_near(output[i], 1.0f, 0.0001f, "default smoothing sample"))
            return 1;
    }

    if (!test_runtime_set_param_by_name(&runtime, "gain", 2.0f))
        return fail("failed to set live smoothed param");
    if (runtime.param_targets[0] != 2.0f || runtime.param_smoothing_remaining_frames[0] != 480u)
        return fail("live smoothed param did not capture target and duration");
    if (!apg_v2_runtime_process_mono(&runtime, input, output, 48u))
        return fail("first live smoothing block failed");
    for (size_t i = 0; i < 48u; i++) {
        if (expect_near(output[i], 1.1f, 0.0001f, "first live smoothing sample"))
            return 1;
    }
    if (expect_near(runtime.params[0], 1.1f, 0.0001f, "first live smoothing param") ||
        runtime.param_smoothing_remaining_frames[0] != 432u)
        return 1;

    if (!apg_v2_runtime_process_mono(&runtime, input, output, 432u))
        return fail("final live smoothing block failed");
    for (size_t i = 0; i < 432u; i++) {
        if (expect_near(output[i], 2.0f, 0.0001f, "final live smoothing sample"))
            return 1;
    }
    if (runtime.param_smoothing_remaining_frames[0] != 0u ||
        expect_near(runtime.params[0], 2.0f, 0.0001f, "final live smoothing param"))
        return 1;

    apg_v2_runtime_destroy(&runtime);
    uc_arena_free(&arena);
    return 0;
}

static int test_named_public_port_signal_lookup(void) {
    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_unit_v2_t          unit;
    apg_v2_compiled_unit_t plan;
    if (load_and_compile_fixture("test/fixtures/units-v2/simple_mix.unit.v2.yaml", &arena, &unit, &plan)) {
        uc_arena_free(&arena);
        return 1;
    }

    apg_v2_runtime_t runtime;
    uc_error         err    = {0};
    uc_status        status = test_apg_v2_runtime_init_registry(&plan, 8u, 48000.0f, &arena, &runtime, &err);
    if (status != UC_OK) {
        fprintf(stderr, "runtime init error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("failed to initialize v2 runtime");
    }

    float *a      = runtime_input_port_signal_for_test(&runtime, "a");
    float *b      = runtime_input_port_signal_for_test(&runtime, "b");
    float *output = runtime_output_port_signal_for_test(&runtime, "output");
    if (!a || !b || !output)
        return fail("failed to find named public port signals");
    if (a != runtime_signal_by_name_for_test(&runtime, "a") || b != runtime_signal_by_name_for_test(&runtime, "b") ||
        output != runtime_signal_by_name_for_test(&runtime, "output"))
        return fail("named public port lookup returned unexpected signal buffer");
    if (runtime_input_port_signal_for_test(&runtime, "output") || runtime_output_port_signal_for_test(&runtime, "a") ||
        runtime_input_port_signal_for_test(&runtime, "missing"))
        return fail("named public port lookup accepted invalid port name or direction");

    a[0] = 0.25f;
    a[1] = -0.5f;
    b[0] = 0.75f;
    b[1] = 0.25f;
    if (!apg_v2_runtime_process(&runtime, 2u))
        return fail("named public port generic processing failed");
    if (output[0] != 1.0f || output[1] != -0.25f)
        return fail("unexpected named public port output sample");

    apg_v2_runtime_destroy(&runtime);
    uc_arena_free(&arena);
    return 0;
}

static int test_multi_output_public_port_process(void) {
    const char *yaml = "kind: apg.unit\n"
                       "schema: apg.unit.v2\n"
                       "name: pan_public_outputs\n"
                       "version: 2.0.0\n"
                       "params:\n"
                       "  pan:\n"
                       "    type: float\n"
                       "    default: 0.25\n"
                       "    min: 0.0\n"
                       "    max: 1.0\n"
                       "ports:\n"
                       "  inputs:\n"
                       "    - name: input\n"
                       "      type: audio\n"
                       "      channels: 1\n"
                       "  outputs:\n"
                       "    - name: left\n"
                       "      type: audio\n"
                       "      channels: 1\n"
                       "    - name: right\n"
                       "      type: audio\n"
                       "      channels: 1\n"
                       "graph:\n"
                       "  signals:\n"
                       "    - input\n"
                       "    - left\n"
                       "    - right\n"
                       "  nodes:\n"
                       "    - id: pan\n"
                       "      atom: mix_pan_stereo\n"
                       "      in:\n"
                       "        signal: input\n"
                       "      out:\n"
                       "        left: left\n"
                       "        right: right\n"
                       "      config:\n"
                       "        position: ${params.pan}\n"
                       "compatibility:\n"
                       "  desktop_full: true\n";

    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_unit_v2_t          unit;
    apg_v2_compiled_unit_t plan;
    if (load_and_compile_string(yaml, &arena, &unit, &plan)) {
        uc_arena_free(&arena);
        return 1;
    }

    apg_v2_runtime_t runtime;
    uc_error         err    = {0};
    uc_status        status = test_apg_v2_runtime_init_registry(&plan, 8u, 48000.0f, &arena, &runtime, &err);
    if (status != UC_OK) {
        fprintf(stderr, "runtime init error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("failed to initialize multi-output runtime");
    }

    float *input = runtime_input_port_signal_for_test(&runtime, "input");
    float *left  = runtime_output_port_signal_for_test(&runtime, "left");
    float *right = runtime_output_port_signal_for_test(&runtime, "right");
    if (!input || !left || !right)
        return fail("failed to find multi-output public port signals");

    input[0] = 2.0f;
    input[1] = -4.0f;
    if (!apg_v2_runtime_process(&runtime, 2u))
        return fail("multi-output generic processing failed");
    if (left[0] != 1.5f || left[1] != -3.0f || right[0] != 0.5f || right[1] != -1.0f)
        return fail("unexpected multi-output samples");

    apg_v2_runtime_destroy(&runtime);
    uc_arena_free(&arena);
    return 0;
}

static int test_named_mono_port_rejects_bad_buffer_layouts(void) {
    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_unit_v2_t          unit;
    apg_v2_compiled_unit_t plan;
    if (load_and_compile_fixture("test/fixtures/units-v2/simple_gain.unit.v2.yaml", &arena, &unit, &plan)) {
        uc_arena_free(&arena);
        return 1;
    }

    apg_v2_runtime_t runtime;
    uc_error         err    = {0};
    uc_status        status = test_apg_v2_runtime_init_registry(&plan, 8u, 48000.0f, &arena, &runtime, &err);
    if (status != UC_OK) {
        fprintf(stderr, "runtime init error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("failed to initialize v2 runtime");
    }

    const float input[2]  = {1.0f, 2.0f};
    float       output[2] = {0.0f, 0.0f};
    if (test_runtime_process_mono_ports(&runtime, "input", NULL, "output", output, 2u))
        return fail("named mono processing accepted null input buffer");
    const char *last_error = apg_v2_measure_last_error(&runtime);
    if (!last_error || !strstr(last_error, "buffers"))
        return fail("null buffer rejection did not expose a useful error");

    if (test_runtime_process_mono_ports(&runtime, "input", input, "input", output, 2u))
        return fail("named mono processing accepted input port as output");
    last_error = apg_v2_measure_last_error(&runtime);
    if (!last_error || !strstr(last_error, "output audio port"))
        return fail("wrong output port rejection did not expose a useful error");

    apg_v2_runtime_destroy(&runtime);
    uc_arena_free(&arena);
    return 0;
}

static int test_interleaved_stereo_public_port_process(void) {
    const char *yaml = "kind: apg.unit\n"
                       "schema: apg.unit.v2\n"
                       "name: stereo_gain_ports\n"
                       "version: 2.0.0\n"
                       "params:\n"
                       "  gain:\n"
                       "    type: float\n"
                       "    default: 2.0\n"
                       "    min: 0.0\n"
                       "    max: 4.0\n"
                       "ports:\n"
                       "  inputs:\n"
                       "    - name: input\n"
                       "      type: audio\n"
                       "      channels: 2\n"
                       "      signals:\n"
                       "        - input_l\n"
                       "        - input_r\n"
                       "  outputs:\n"
                       "    - name: output\n"
                       "      type: audio\n"
                       "      channels: 2\n"
                       "      signals:\n"
                       "        - output_l\n"
                       "        - output_r\n"
                       "graph:\n"
                       "  signals:\n"
                       "    - input_l\n"
                       "    - input_r\n"
                       "    - gain_value\n"
                       "    - output_l\n"
                       "    - output_r\n"
                       "  nodes:\n"
                       "    - id: gain_value\n"
                       "      atom: generation_dc\n"
                       "      out:\n"
                       "        signal: gain_value\n"
                       "      config:\n"
                       "        value: ${params.gain}\n"
                       "    - id: gain_left\n"
                       "      atom: amplitude_multiply\n"
                       "      in:\n"
                       "        signal_a: input_l\n"
                       "        signal_b: gain_value\n"
                       "      out:\n"
                       "        signal: output_l\n"
                       "    - id: gain_right\n"
                       "      atom: amplitude_multiply\n"
                       "      in:\n"
                       "        signal_a: input_r\n"
                       "        signal_b: gain_value\n"
                       "      out:\n"
                       "        signal: output_r\n"
                       "compatibility:\n"
                       "  desktop_full: true\n";

    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_unit_v2_t          unit;
    apg_v2_compiled_unit_t plan;
    if (load_and_compile_string(yaml, &arena, &unit, &plan)) {
        uc_arena_free(&arena);
        return 1;
    }

    apg_v2_runtime_t runtime;
    uc_error         err    = {0};
    uc_status        status = test_apg_v2_runtime_init_registry(&plan, 8u, 48000.0f, &arena, &runtime, &err);
    if (status != UC_OK) {
        fprintf(stderr, "runtime init error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("failed to initialize stereo runtime");
    }

    if (runtime_port_channel_for_test(&runtime, "input", 1u, false) !=
        runtime_signal_by_name_for_test(&runtime, "input_r"))
        return fail("stereo input channel lookup returned unexpected signal");
    if (runtime_port_channel_for_test(&runtime, "output", 1u, true) !=
        runtime_signal_by_name_for_test(&runtime, "output_r"))
        return fail("stereo output channel lookup returned unexpected signal");
    if (runtime_port_channel_for_test(&runtime, "input", 2u, false))
        return fail("stereo input channel lookup accepted out-of-range channel");
    size_t input_port_index  = 0u;
    size_t output_port_index = 0u;
    if (!test_runtime_input_audio_port_index_by_name(&runtime, "input", &input_port_index) ||
        !test_runtime_output_audio_port_index_by_name(&runtime, "output", &output_port_index))
        return fail("stereo port index resolution failed");
    size_t input_r_index  = 0u;
    size_t output_r_index = 0u;
    if (!apg_v2_runtime_input_port_channel_signal_index(&runtime, input_port_index, 1u, &input_r_index, NULL) ||
        !apg_v2_runtime_output_port_channel_signal_index(&runtime, output_port_index, 1u, &output_r_index, NULL))
        return fail("stereo channel signal index resolution failed");
    if (apg_v2_runtime_signal_buffer_at(&runtime, input_r_index) !=
            runtime_signal_by_name_for_test(&runtime, "input_r") ||
        apg_v2_runtime_signal_buffer_at(&runtime, output_r_index) !=
            runtime_signal_by_name_for_test(&runtime, "output_r"))
        return fail("stereo channel indices mapped unexpected signal buffers");

    const float input[4]  = {1.0f, 10.0f, -2.0f, -20.0f};
    float       output[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    if (!apg_v2_runtime_process_interleaved_port_indices(
            &runtime, input_port_index, input, output_port_index, output, 2u
        ))
        return fail("stereo interleaved index processing failed");
    const float expected[4] = {2.0f, 20.0f, -4.0f, -40.0f};
    for (size_t i = 0; i < 4u; i++) {
        if (output[i] != expected[i])
            return fail("unexpected stereo interleaved output sample");
    }

    apg_v2_runtime_destroy(&runtime);
    uc_arena_free(&arena);
    return 0;
}

static int test_named_mono_port_process(void) {
    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_unit_v2_t          unit;
    apg_v2_compiled_unit_t plan;
    if (load_and_compile_fixture("test/fixtures/units-v2/simple_gain.unit.v2.yaml", &arena, &unit, &plan)) {
        uc_arena_free(&arena);
        return 1;
    }

    apg_v2_runtime_t runtime;
    uc_error         err    = {0};
    uc_status        status = test_apg_v2_runtime_init_registry(&plan, 8u, 48000.0f, &arena, &runtime, &err);
    if (status != UC_OK) {
        fprintf(stderr, "runtime init error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("failed to initialize v2 runtime");
    }

    if (!apg_v2_runtime_set_param_index(&runtime, 0u, 3.0f))
        return fail("failed to set simple_gain param by index");
    const float input[3]          = {0.25f, -0.5f, 1.0f};
    float       output[3]         = {0.0f, 0.0f, 0.0f};
    size_t      input_port_index  = 0u;
    size_t      output_port_index = 0u;
    if (!test_runtime_input_audio_port_index_by_name(&runtime, "input", &input_port_index) ||
        !test_runtime_output_audio_port_index_by_name(&runtime, "output", &output_port_index))
        return fail("simple_gain port index resolution failed");
    if (!apg_v2_runtime_process_mono_port_indices(&runtime, input_port_index, input, output_port_index, output, 3u))
        return fail("indexed simple_gain processing failed");
    if (output[0] != 0.75f || output[1] != -1.5f || output[2] != 3.0f)
        return fail("unexpected named simple_gain output sample");

    if (test_runtime_process_mono_ports(&runtime, "missing", input, "output", output, 3u))
        return fail("named simple_gain accepted missing input port");
    const char *last_error = apg_v2_measure_last_error(&runtime);
    if (!last_error || !strstr(last_error, "input audio port"))
        return fail("missing input port failure did not expose a useful error");

    apg_v2_runtime_destroy(&runtime);
    uc_arena_free(&arena);
    return 0;
}

static int test_control_port_sets_matching_param(void) {
    const char *yaml = "kind: apg.unit\n"
                       "schema: apg.unit.v2\n"
                       "name: gain_control_port\n"
                       "version: 2.0.0\n"
                       "params:\n"
                       "  gain:\n"
                       "    type: float\n"
                       "    default: 1.0\n"
                       "    min: 0.0\n"
                       "    max: 4.0\n"
                       "ports:\n"
                       "  inputs:\n"
                       "    - name: input\n"
                       "      type: audio\n"
                       "      channels: 1\n"
                       "    - name: gain\n"
                       "      type: control\n"
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

    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_unit_v2_t unit;
    uc_error      err    = {0};
    uc_status     status = apg_unit_v2_load_string(yaml, strlen(yaml), &arena, &unit, &err);
    if (status != UC_OK) {
        fprintf(stderr, "load error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("failed to load control port fixture");
    }

    apg_v2_compiled_unit_t plan;
    status = apg_v2_compile_unit(&unit, &arena, &plan, &err);
    if (status != UC_OK) {
        fprintf(stderr, "compile error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("failed to compile control port fixture");
    }

    apg_v2_runtime_t runtime;
    status = test_apg_v2_runtime_init_registry(&plan, 8u, 48000.0f, &arena, &runtime, &err);
    if (status != UC_OK) {
        fprintf(stderr, "runtime init error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("failed to initialize control port runtime");
    }

    if (!test_runtime_set_control_port_by_name(&runtime, "gain", 2.0f))
        return fail("failed to set matching control port");
    if (!apg_v2_runtime_set_control_port_index(&runtime, 1u, 4.0f))
        return fail("failed to set targeted control port by index");
    if (apg_v2_runtime_set_control_port_index(&runtime, 2u, 1.0f))
        return fail("accepted invalid control target index");
    if (test_runtime_set_control_port_by_name(&runtime, "input", 2.0f) ||
        test_runtime_set_control_port_by_name(&runtime, "missing", 2.0f))
        return fail("accepted invalid control port update");

    const float input[2]  = {0.25f, -0.5f};
    float       output[2] = {0.0f, 0.0f};
    if (!test_runtime_process_mono_ports(&runtime, "input", input, "output", output, 2u))
        return fail("control port fixture processing failed");
    if (output[0] != 1.0f || output[1] != -2.0f)
        return fail("control port did not update matching param");

    apg_v2_runtime_destroy(&runtime);
    uc_arena_free(&arena);
    return 0;
}

static int test_named_mono_port_rejects_multichannel_port(void) {
    const char *yaml = "kind: apg.unit\n"
                       "schema: apg.unit.v2\n"
                       "name: stereo_public_port\n"
                       "version: 2.0.0\n"
                       "params:\n"
                       "  gain:\n"
                       "    type: float\n"
                       "    default: 1.0\n"
                       "    min: 0.0\n"
                       "    max: 2.0\n"
                       "ports:\n"
                       "  inputs:\n"
                       "    - name: input\n"
                       "      type: audio\n"
                       "      channels: 2\n"
                       "      signals:\n"
                       "        - input_l\n"
                       "        - input_r\n"
                       "  outputs:\n"
                       "    - name: output\n"
                       "      type: audio\n"
                       "      channels: 1\n"
                       "graph:\n"
                       "  signals:\n"
                       "    - input_l\n"
                       "    - input_r\n"
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
                       "        signal_a: input_l\n"
                       "        signal_b: gain_value\n"
                       "      out:\n"
                       "        signal: output\n"
                       "compatibility:\n"
                       "  desktop_full: true\n";

    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_unit_v2_t unit;
    uc_error      err    = {0};
    uc_status     status = apg_unit_v2_load_string(yaml, strlen(yaml), &arena, &unit, &err);
    if (status != UC_OK) {
        fprintf(stderr, "load error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("failed to load stereo public port fixture");
    }

    apg_v2_compiled_unit_t plan;
    status = apg_v2_compile_unit(&unit, &arena, &plan, &err);
    if (status != UC_OK) {
        fprintf(stderr, "compile error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("failed to compile stereo public port fixture");
    }

    apg_v2_runtime_t runtime;
    status = test_apg_v2_runtime_init_registry(&plan, 8u, 48000.0f, &arena, &runtime, &err);
    if (status != UC_OK) {
        fprintf(stderr, "runtime init error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("failed to initialize stereo public port runtime");
    }

    const float input[2]  = {1.0f, 2.0f};
    float       output[2] = {0.0f, 0.0f};
    if (test_runtime_process_mono_ports(&runtime, "input", input, "output", output, 2u))
        return fail("named mono processing accepted a multi-channel input port");
    const char *last_error = apg_v2_measure_last_error(&runtime);
    if (!last_error || !strstr(last_error, "mono audio ports"))
        return fail("multi-channel port rejection did not expose a useful error");

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
    if (load_and_compile_fixture("test/fixtures/units-v2/simple_clip.unit.v2.yaml", &arena, &unit, &plan)) {
        uc_arena_free(&arena);
        return 1;
    }

    apg_v2_runtime_t runtime;
    uc_error         err    = {0};
    uc_status        status = test_apg_v2_runtime_init_registry(&plan, 8u, 48000.0f, &arena, &runtime, &err);
    if (status != UC_OK) {
        fprintf(stderr, "runtime init error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("failed to initialize v2 runtime");
    }

    if (!test_runtime_set_param_by_name(&runtime, "gain", 2.0f))
        return fail("failed to set simple_clip param");
    float *input  = runtime_signal_by_name_for_test(&runtime, "input");
    float *output = runtime_signal_by_name_for_test(&runtime, "output");
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
    if (runtime_signal_by_name_for_test(&runtime, "missing"))
        return fail("missing signal lookup unexpectedly succeeded");
    if (test_runtime_set_param_by_name(&runtime, "missing", 1.0f))
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
    if (load_and_compile_fixture("test/fixtures/units-v2/simple_mix.unit.v2.yaml", &arena, &unit, &plan)) {
        uc_arena_free(&arena);
        return 1;
    }

    apg_v2_runtime_t runtime;
    uc_error         err    = {0};
    uc_status        status = test_apg_v2_runtime_init_registry(&plan, 8u, 48000.0f, &arena, &runtime, &err);
    if (status != UC_OK) {
        fprintf(stderr, "runtime init error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("failed to initialize v2 runtime");
    }

    float *a      = runtime_signal_by_name_for_test(&runtime, "a");
    float *b      = runtime_signal_by_name_for_test(&runtime, "b");
    float *output = runtime_signal_by_name_for_test(&runtime, "output");
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

static int test_mix_matrix_process_generic(void) {
    const char *yaml = "kind: apg.unit\n"
                       "schema: apg.unit.v2\n"
                       "name: matrix_mix_runtime\n"
                       "version: 2.0.0\n"
                       "params:\n"
                       "  bypass:\n"
                       "    type: bool\n"
                       "    default: false\n"
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
    if (load_and_compile_string(yaml, &arena, &unit, &plan)) {
        uc_arena_free(&arena);
        return 1;
    }

    apg_v2_runtime_t runtime;
    uc_error         err    = {0};
    uc_status        status = test_apg_v2_runtime_init_registry(&plan, 8u, 48000.0f, &arena, &runtime, &err);
    if (status != UC_OK) {
        fprintf(stderr, "runtime init error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("failed to initialize mix_matrix runtime");
    }

    float *a    = runtime_signal_by_name_for_test(&runtime, "a");
    float *b    = runtime_signal_by_name_for_test(&runtime, "b");
    float *sum  = runtime_signal_by_name_for_test(&runtime, "sum");
    float *diff = runtime_signal_by_name_for_test(&runtime, "diff");
    if (!a || !b || !sum || !diff)
        return fail("failed to find mix_matrix signals");

    a[0] = 2.0f;
    a[1] = -4.0f;
    b[0] = 6.0f;
    b[1] = 1.0f;
    if (!apg_v2_runtime_process(&runtime, 2u))
        return fail("mix_matrix processing failed");
    const float expected_sum[2]  = {4.0f, -1.5f};
    const float expected_diff[2] = {-4.0f, -5.0f};
    if (expect_samples(sum, expected_sum, 2u, "mix_matrix sum") ||
        expect_samples(diff, expected_diff, 2u, "mix_matrix diff"))
        return 1;

    apg_v2_runtime_destroy(&runtime);
    uc_arena_free(&arena);
    return 0;
}

static int test_delay_line_state_buffer_process(void) {
    const char *yaml = "kind: apg.unit\n"
                       "schema: apg.unit.v2\n"
                       "name: delay_state_buffer\n"
                       "version: 2.0.0\n"
                       "params:\n"
                       "  bypass:\n"
                       "    type: bool\n"
                       "    default: false\n"
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
                       "    - id: delay\n"
                       "      atom: delay_line\n"
                       "      in:\n"
                       "        signal: input\n"
                       "      out:\n"
                       "        signal: output\n"
                       "      config:\n"
                       "        length: 2\n"
                       "compatibility:\n"
                       "  desktop_full: true\n";

    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_unit_v2_t unit;
    uc_error      err    = {0};
    uc_status     status = apg_unit_v2_load_string(yaml, strlen(yaml), &arena, &unit, &err);
    if (status != UC_OK) {
        fprintf(stderr, "load error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("failed to load delay state fixture");
    }

    apg_v2_compiled_unit_t plan;
    status = apg_v2_compile_unit(&unit, &arena, &plan, &err);
    if (status != UC_OK) {
        fprintf(stderr, "compile error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("failed to compile delay state fixture");
    }

    apg_v2_runtime_t runtime;
    status = test_apg_v2_runtime_init_registry(&plan, 8u, 48000.0f, &arena, &runtime, &err);
    if (status != UC_OK) {
        fprintf(stderr, "runtime init error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("failed to initialize delay state runtime");
    }
    if (runtime.nodes_len != 1u || runtime.nodes[0].state_buffers_len != 1u || !runtime.nodes[0].state_buffers[0])
        return fail("delay_line state buffer was not allocated");

    float *input  = runtime_signal_by_name_for_test(&runtime, "input");
    float *output = runtime_signal_by_name_for_test(&runtime, "output");
    if (!input || !output)
        return fail("failed to find delay state signals");

    input[0] = 1.0f;
    input[1] = 2.0f;
    input[2] = 3.0f;
    input[3] = 4.0f;
    if (!apg_v2_runtime_process(&runtime, 4u))
        return fail("delay_line state processing failed");

    const float expected[4] = {0.0f, 0.0f, 1.0f, 2.0f};
    for (size_t i = 0; i < 4u; i++) {
        if (output[i] != expected[i])
            return fail("unexpected delay_line output sample");
    }

    if (!apg_v2_runtime_reset(&runtime))
        return fail("failed to reset delay_line runtime");
    input[0] = 1.0f;
    input[1] = 2.0f;
    input[2] = 3.0f;
    input[3] = 4.0f;
    if (!apg_v2_runtime_process(&runtime, 4u))
        return fail("delay_line state processing after reset failed");
    for (size_t i = 0; i < 4u; i++) {
        if (output[i] != expected[i])
            return fail("delay_line reset did not clear state");
    }

    apg_v2_runtime_destroy(&runtime);
    uc_arena_free(&arena);
    return 0;
}

static int test_filter_state_buffer_uses_descriptor_capacity(void) {
    const char *yaml = "kind: apg.unit\n"
                       "schema: apg.unit.v2\n"
                       "name: filter_state_buffer_capacity\n"
                       "version: 2.0.0\n"
                       "params:\n"
                       "  bypass:\n"
                       "    type: bool\n"
                       "    default: false\n"
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
                       "    - id: comb\n"
                       "      atom: filter_comb_ff\n"
                       "      in:\n"
                       "        signal: input\n"
                       "      out:\n"
                       "        signal: output\n"
                       "      config:\n"
                       "        delay_samples: 2\n"
                       "        coefficient: 0.5\n"
                       "compatibility:\n"
                       "  desktop_full: true\n";

    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_unit_v2_t          unit;
    apg_v2_compiled_unit_t plan;
    if (load_and_compile_string(yaml, &arena, &unit, &plan)) {
        uc_arena_free(&arena);
        return 1;
    }

    apg_v2_runtime_t runtime;
    uc_error         err    = {0};
    uc_status        status = test_apg_v2_runtime_init_registry(&plan, 8u, 48000.0f, &arena, &runtime, &err);
    if (status != UC_OK) {
        fprintf(stderr, "runtime init error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("failed to initialize filter state runtime");
    }
    if (runtime.nodes_len != 1u || runtime.nodes[0].state_buffers_len != 1u || !runtime.nodes[0].state_buffers[0])
        return fail("filter_comb_ff state buffer was not allocated");
    if (!runtime.nodes[0].state_buffer_samples || runtime.nodes[0].state_buffer_samples[0] != 48000u)
        return fail("filter_comb_ff state buffer did not use descriptor capacity");

    apg_v2_runtime_destroy(&runtime);
    uc_arena_free(&arena);
    return 0;
}

static int test_delay_tap_scalar_input_refresh(void) {
    const char *yaml = "kind: apg.unit\n"
                       "schema: apg.unit.v2\n"
                       "name: delay_tap_runtime\n"
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
    if (load_and_compile_string(yaml, &arena, &unit, &plan)) {
        uc_arena_free(&arena);
        return 1;
    }

    apg_v2_runtime_t runtime;
    uc_error         err    = {0};
    uc_status        status = test_apg_v2_runtime_init_registry(&plan, 8u, 48000.0f, &arena, &runtime, &err);
    if (status != UC_OK) {
        fprintf(stderr, "runtime init error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("failed to initialize delay tap runtime");
    }

    float *input  = runtime_signal_by_name_for_test(&runtime, "input");
    float *output = runtime_signal_by_name_for_test(&runtime, "output");
    if (!input || !output)
        return fail("failed to find delay tap signals");

    input[0] = 1.0f;
    input[1] = 2.0f;
    input[2] = 3.0f;
    input[3] = 4.0f;
    if (!apg_v2_runtime_process(&runtime, 4u))
        return fail("delay tap processing failed");
    const float expected_tap_2[4] = {1.5f, 1.5f, 1.5f, 1.5f};
    if (expect_samples(output, expected_tap_2, 4u, "delay tap default"))
        return 1;

    if (!test_runtime_set_param_by_name(&runtime, "tap", 1.0f))
        return fail("failed to update delay tap param");
    input[0] = 1.0f;
    input[1] = 2.0f;
    input[2] = 3.0f;
    input[3] = 4.0f;
    if (!apg_v2_runtime_process(&runtime, 4u))
        return fail("delay tap processing after param update failed");
    const float expected_tap_1[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    if (expect_samples(output, expected_tap_1, 4u, "delay tap updated"))
        return 1;

    apg_v2_runtime_destroy(&runtime);
    uc_arena_free(&arena);
    return 0;
}

static int test_product_fixture_library_runtime_smoke(void) {
    const char *mono_fixtures[] = {
        "test/fixtures/units-v2/overdrive.unit.v2.yaml",  "test/fixtures/units-v2/delay.unit.v2.yaml",
        "test/fixtures/units-v2/tremolo.unit.v2.yaml",    "test/fixtures/units-v2/tone_stack.unit.v2.yaml",
        "test/fixtures/units-v2/noise_gate.unit.v2.yaml",
    };
    const float input[4] = {0.1f, 0.25f, -0.5f, 0.75f};

    for (size_t i = 0; i < sizeof(mono_fixtures) / sizeof(mono_fixtures[0]); i++) {
        uc_arena arena;
        if (uc_arena_init(&arena, 1024 * 1024) != 0)
            return fail("arena init failed");

        apg_unit_v2_t          unit;
        apg_v2_compiled_unit_t plan;
        if (load_and_compile_fixture(mono_fixtures[i], &arena, &unit, &plan)) {
            uc_arena_free(&arena);
            return 1;
        }

        apg_v2_runtime_t runtime;
        uc_error         err = {0};
        if (test_apg_v2_runtime_init_registry(&plan, 8u, 48000.0f, &arena, &runtime, &err) != UC_OK) {
            fprintf(stderr, "runtime init error: %s\n", err.msg);
            uc_arena_free(&arena);
            return fail("failed to initialize product fixture runtime");
        }

        float output[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        if (!test_runtime_process_mono_ports(&runtime, "input", input, "output", output, 4u))
            return fail("product fixture processing failed");
        if (expect_finite_samples(output, 4u, mono_fixtures[i]))
            return 1;

        apg_v2_runtime_destroy(&runtime);
        uc_arena_free(&arena);
    }

    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_unit_v2_t          unit;
    apg_v2_compiled_unit_t plan;
    if (load_and_compile_fixture("test/fixtures/units-v2/wet_dry_mix.unit.v2.yaml", &arena, &unit, &plan)) {
        uc_arena_free(&arena);
        return 1;
    }

    apg_v2_runtime_t runtime;
    uc_error         err = {0};
    if (test_apg_v2_runtime_init_registry(&plan, 8u, 48000.0f, &arena, &runtime, &err) != UC_OK) {
        fprintf(stderr, "runtime init error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("failed to initialize wet/dry fixture runtime");
    }

    float *dry    = runtime_input_port_signal_for_test(&runtime, "dry");
    float *wet    = runtime_input_port_signal_for_test(&runtime, "wet");
    float *output = runtime_output_port_signal_for_test(&runtime, "output");
    if (!dry || !wet || !output)
        return fail("wet/dry fixture public ports are missing");
    dry[0] = 0.0f;
    dry[1] = 1.0f;
    wet[0] = 1.0f;
    wet[1] = 0.0f;
    if (!apg_v2_runtime_process(&runtime, 2u))
        return fail("wet/dry fixture processing failed");
    const float expected[2] = {0.5f, 0.5f};
    if (expect_samples(output, expected, 2u, "wet/dry fixture"))
        return 1;

    apg_v2_runtime_destroy(&runtime);
    uc_arena_free(&arena);
    return 0;
}

static int test_runtime_capable_fixture_library(void) {
    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_unit_v2_t          unit;
    apg_v2_compiled_unit_t plan;
    apg_v2_runtime_t       runtime;
    uc_error               err = {0};

    if (load_and_compile_fixture("test/fixtures/units-v2/delay_line_state.unit.v2.yaml", &arena, &unit, &plan)) {
        uc_arena_free(&arena);
        return 1;
    }
    if (test_apg_v2_runtime_init_registry(&plan, 8u, 48000.0f, &arena, &runtime, &err) != UC_OK) {
        fprintf(stderr, "runtime init error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("failed to initialize delay fixture runtime");
    }
    float *input  = runtime_signal_by_name_for_test(&runtime, "input");
    float *output = runtime_signal_by_name_for_test(&runtime, "output");
    if (!input || !output)
        return fail("delay fixture signals are missing");
    input[0] = 1.0f;
    input[1] = 2.0f;
    input[2] = 3.0f;
    input[3] = 4.0f;
    if (!apg_v2_runtime_process(&runtime, 4u))
        return fail("delay fixture processing failed");
    const float delay_expected[4] = {0.0f, 0.0f, 1.0f, 2.0f};
    if (expect_samples(output, delay_expected, 4u, "delay fixture"))
        return 1;
    if (!apg_v2_runtime_reset(&runtime))
        return fail("delay fixture reset failed");
    input[0] = 1.0f;
    input[1] = 2.0f;
    input[2] = 3.0f;
    input[3] = 4.0f;
    if (!apg_v2_runtime_process(&runtime, 4u) || expect_samples(output, delay_expected, 4u, "delay reset fixture"))
        return fail("delay fixture reset did not restore deterministic output");
    apg_v2_runtime_destroy(&runtime);
    uc_arena_free(&arena);

    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");
    if (load_and_compile_fixture("test/fixtures/units-v2/filter_comb_ff_state.unit.v2.yaml", &arena, &unit, &plan)) {
        uc_arena_free(&arena);
        return 1;
    }
    if (test_apg_v2_runtime_init_registry(&plan, 8u, 48000.0f, &arena, &runtime, &err) != UC_OK) {
        fprintf(stderr, "runtime init error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("failed to initialize filter fixture runtime");
    }
    input  = runtime_signal_by_name_for_test(&runtime, "input");
    output = runtime_signal_by_name_for_test(&runtime, "output");
    if (!input || !output)
        return fail("filter fixture signals are missing");
    input[0] = 1.0f;
    input[1] = 2.0f;
    input[2] = 3.0f;
    input[3] = 4.0f;
    if (!apg_v2_runtime_process(&runtime, 4u))
        return fail("filter fixture processing failed");
    const float filter_expected[4] = {1.0f, 2.0f, 3.5f, 5.0f};
    if (expect_samples(output, filter_expected, 4u, "filter fixture"))
        return 1;
    if (!apg_v2_runtime_reset(&runtime))
        return fail("filter fixture reset failed");
    input[0] = 1.0f;
    input[1] = 2.0f;
    input[2] = 3.0f;
    input[3] = 4.0f;
    if (!apg_v2_runtime_process(&runtime, 4u) || expect_samples(output, filter_expected, 4u, "filter reset fixture"))
        return fail("filter fixture reset did not restore deterministic output");
    apg_v2_runtime_destroy(&runtime);
    uc_arena_free(&arena);

    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");
    if (load_and_compile_fixture(
            "test/fixtures/units-v2/modulation_frequency_state.unit.v2.yaml", &arena, &unit, &plan
        )) {
        uc_arena_free(&arena);
        return 1;
    }
    if (test_apg_v2_runtime_init_registry(&plan, 8u, 48000.0f, &arena, &runtime, &err) != UC_OK) {
        fprintf(stderr, "runtime init error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("failed to initialize modulation fixture runtime");
    }
    input            = runtime_signal_by_name_for_test(&runtime, "input");
    float *modulator = runtime_signal_by_name_for_test(&runtime, "modulator");
    output           = runtime_signal_by_name_for_test(&runtime, "output");
    if (!input || !modulator || !output)
        return fail("modulation fixture signals are missing");
    for (size_t i = 0; i < 4u; i++) {
        input[i]     = (float)(i + 1u);
        modulator[i] = 1.0f;
    }
    if (!apg_v2_runtime_process(&runtime, 4u))
        return fail("modulation fixture processing failed");
    const float modulation_expected[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    if (expect_samples(output, modulation_expected, 4u, "modulation fixture"))
        return 1;
    modulation_frequency_state_t *mod_state  = (modulation_frequency_state_t *)runtime.nodes[0].state_storage;
    float                        *mod_buffer = mod_state->buffer;
    if (!mod_buffer || mod_state->write_pos != 4 || mod_state->current_delay != 4.0f)
        return fail("modulation fixture state did not advance deterministically");
    if (!apg_v2_runtime_reset(&runtime))
        return fail("modulation fixture reset failed");
    if (mod_state->buffer != mod_buffer || mod_state->write_pos != 0 || mod_state->current_delay != 0.0f)
        return fail("modulation fixture reset did not restore state storage");
    apg_v2_runtime_destroy(&runtime);
    uc_arena_free(&arena);

    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");
    if (load_and_compile_fixture("test/fixtures/units-v2/stereo_pan.unit.v2.yaml", &arena, &unit, &plan)) {
        uc_arena_free(&arena);
        return 1;
    }
    if (test_apg_v2_runtime_init_registry(&plan, 8u, 48000.0f, &arena, &runtime, &err) != UC_OK) {
        fprintf(stderr, "runtime init error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("failed to initialize stereo fixture runtime");
    }
    input        = runtime_input_port_signal_for_test(&runtime, "input");
    float *left  = runtime_output_port_signal_for_test(&runtime, "left");
    float *right = runtime_output_port_signal_for_test(&runtime, "right");
    if (!input || !left || !right)
        return fail("stereo fixture public ports are missing");
    input[0] = 2.0f;
    input[1] = -4.0f;
    if (!apg_v2_runtime_process(&runtime, 2u))
        return fail("stereo fixture processing failed");
    const float left_expected[2]  = {1.5f, -3.0f};
    const float right_expected[2] = {0.5f, -1.0f};
    if (expect_samples(left, left_expected, 2u, "stereo left fixture") ||
        expect_samples(right, right_expected, 2u, "stereo right fixture"))
        return 1;
    apg_v2_runtime_destroy(&runtime);
    uc_arena_free(&arena);

    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");
    if (load_and_compile_fixture("test/fixtures/units-v2/control_gain.unit.v2.yaml", &arena, &unit, &plan)) {
        uc_arena_free(&arena);
        return 1;
    }
    if (test_apg_v2_runtime_init_registry(&plan, 8u, 48000.0f, &arena, &runtime, &err) != UC_OK) {
        fprintf(stderr, "runtime init error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("failed to initialize control fixture runtime");
    }
    if (!test_runtime_set_control_port_by_name(&runtime, "amount", 3.0f))
        return fail("control fixture did not accept target control port");
    const float control_input[2]  = {0.25f, -0.5f};
    float       control_output[2] = {0.0f, 0.0f};
    if (!test_runtime_process_mono_ports(&runtime, "input", control_input, "output", control_output, 2u))
        return fail("control fixture processing failed");
    const float control_expected[2] = {0.75f, -1.5f};
    if (expect_samples(control_output, control_expected, 2u, "control fixture"))
        return 1;
    apg_v2_runtime_destroy(&runtime);
    uc_arena_free(&arena);

    return 0;
}

static int test_runtime_init_failure_cleans_partial_allocations(void) {
    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    const char   *signal_names[] = {"input"};
    apg_unit_v2_t unit           = {
                  .name        = "bad_runtime",
                  .version     = "2.0.0",
                  .signals     = signal_names,
                  .signals_len = 1u,
    };
    apg_v2_compiled_node_t nodes[1] = {{0}};
    nodes[0].id                     = "bad_node";
    nodes[0].atom                   = NULL;
    apg_v2_compiled_unit_t plan     = {
            .unit      = &unit,
            .nodes     = nodes,
            .nodes_len = 1u,
    };

    apg_v2_runtime_t runtime = {0};
    uc_error         err     = {0};
    uc_status        status  = test_apg_v2_runtime_init_registry(&plan, 8u, 48000.0f, &arena, &runtime, &err);
    if (status == UC_OK) {
        uc_arena_free(&arena);
        return fail("runtime init accepted node without atom metadata");
    }
    if (runtime.signal_pool || runtime.signals || runtime.params || runtime.param_targets ||
        runtime.param_smoothing_remaining_frames || runtime.bypassed_instances || runtime.nodes ||
        runtime.signals_len != 0u || runtime.nodes_len != 0u) {
        uc_arena_free(&arena);
        return fail("runtime init failure did not clean partial allocations");
    }
    if (!strstr(err.msg, "compiled atom layout")) {
        uc_arena_free(&arena);
        return fail("runtime init failure did not report useful error");
    }
    uc_arena_free(&arena);
    return 0;
}

static int test_runtime_init_rejects_zero_capacity(void) {
    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_unit_v2_t          unit;
    apg_v2_compiled_unit_t plan;
    if (load_and_compile_fixture("test/fixtures/units-v2/simple_gain.unit.v2.yaml", &arena, &unit, &plan)) {
        uc_arena_free(&arena);
        return 1;
    }

    apg_v2_runtime_t runtime;
    uc_error         err    = {0};
    uc_status        status = test_apg_v2_runtime_init_registry(&plan, 0u, 48000.0f, &arena, &runtime, &err);
    uc_arena_free(&arena);
    if (status == UC_OK)
        return fail("runtime accepted zero frame capacity");
    return 0;
}

int main(void) {
    if (test_runtime_init_simple_gain())
        return 1;
    if (test_v2_host_file_bridge_processes_simple_gain())
        return 1;
    if (test_runtime_config_error_names_node_atom_and_binding())
        return 1;
    if (test_simple_gain_process_mono())
        return 1;
    if (test_runtime_param_smoothing_advances_at_block_boundaries())
        return 1;
    if (test_named_public_port_signal_lookup())
        return 1;
    if (test_multi_output_public_port_process())
        return 1;
    if (test_named_mono_port_rejects_bad_buffer_layouts())
        return 1;
    if (test_interleaved_stereo_public_port_process())
        return 1;
    if (test_named_mono_port_process())
        return 1;
    if (test_named_mono_port_rejects_multichannel_port())
        return 1;
    if (test_control_port_sets_matching_param())
        return 1;
    if (test_simple_clip_process_generic())
        return 1;
    if (test_simple_mix_process_generic())
        return 1;
    if (test_mix_matrix_process_generic())
        return 1;
    if (test_delay_line_state_buffer_process())
        return 1;
    if (test_delay_tap_scalar_input_refresh())
        return 1;
    if (test_filter_state_buffer_uses_descriptor_capacity())
        return 1;
    if (test_product_fixture_library_runtime_smoke())
        return 1;
    if (test_runtime_capable_fixture_library())
        return 1;
    if (test_runtime_init_failure_cleans_partial_allocations())
        return 1;
    if (test_runtime_init_rejects_zero_capacity())
        return 1;
    return 0;
}
