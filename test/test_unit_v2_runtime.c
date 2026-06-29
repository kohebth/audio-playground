#include <apgcore/compiler_v2.h>
#include <apgcore/runtime_v2.h>
#include <apgcore/unit_v2.h>

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
    const char *last_error = apg_v2_runtime_last_error(&runtime);
    if (!last_error || !strstr(last_error, "capacity"))
        return fail("simple_gain over-capacity failure did not expose a useful error");

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

    float *a      = apg_v2_runtime_find_input_port_signal(&runtime, "a");
    float *b      = apg_v2_runtime_find_input_port_signal(&runtime, "b");
    float *output = apg_v2_runtime_find_output_port_signal(&runtime, "output");
    if (!a || !b || !output)
        return fail("failed to find named public port signals");
    if (a != apg_v2_runtime_find_signal(&runtime, "a") || b != apg_v2_runtime_find_signal(&runtime, "b") ||
        output != apg_v2_runtime_find_signal(&runtime, "output"))
        return fail("named public port lookup returned unexpected signal buffer");
    if (apg_v2_runtime_find_input_port_signal(&runtime, "output") ||
        apg_v2_runtime_find_output_port_signal(&runtime, "a") ||
        apg_v2_runtime_find_input_port_signal(&runtime, "missing"))
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
    uc_status        status = apg_v2_runtime_init(&plan, 8u, 48000.0f, &runtime, &err);
    if (status != UC_OK) {
        fprintf(stderr, "runtime init error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("failed to initialize multi-output runtime");
    }

    float *input = apg_v2_runtime_find_input_port_signal(&runtime, "input");
    float *left  = apg_v2_runtime_find_output_port_signal(&runtime, "left");
    float *right = apg_v2_runtime_find_output_port_signal(&runtime, "right");
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

    const float input[2]  = {1.0f, 2.0f};
    float       output[2] = {0.0f, 0.0f};
    if (apg_v2_runtime_process_mono_ports(&runtime, "input", NULL, "output", output, 2u))
        return fail("named mono processing accepted null input buffer");
    const char *last_error = apg_v2_runtime_last_error(&runtime);
    if (!last_error || !strstr(last_error, "buffers"))
        return fail("null buffer rejection did not expose a useful error");

    if (apg_v2_runtime_process_mono_ports(&runtime, "input", input, "input", output, 2u))
        return fail("named mono processing accepted input port as output");
    last_error = apg_v2_runtime_last_error(&runtime);
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
    uc_status        status = apg_v2_runtime_init(&plan, 8u, 48000.0f, &runtime, &err);
    if (status != UC_OK) {
        fprintf(stderr, "runtime init error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("failed to initialize stereo runtime");
    }

    if (apg_v2_runtime_find_input_port_channel_signal(&runtime, "input", 1u) !=
        apg_v2_runtime_find_signal(&runtime, "input_r"))
        return fail("stereo input channel lookup returned unexpected signal");
    if (apg_v2_runtime_find_output_port_channel_signal(&runtime, "output", 1u) !=
        apg_v2_runtime_find_signal(&runtime, "output_r"))
        return fail("stereo output channel lookup returned unexpected signal");
    if (apg_v2_runtime_find_input_port_channel_signal(&runtime, "input", 2u))
        return fail("stereo input channel lookup accepted out-of-range channel");

    const float input[4]  = {1.0f, 10.0f, -2.0f, -20.0f};
    float       output[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    if (!apg_v2_runtime_process_interleaved_ports(&runtime, "input", input, "output", output, 2u))
        return fail("stereo interleaved processing failed");
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

    if (!apg_v2_runtime_set_param(&runtime, "gain", 3.0f))
        return fail("failed to set simple_gain param");
    const float input[3]  = {0.25f, -0.5f, 1.0f};
    float       output[3] = {0.0f, 0.0f, 0.0f};
    if (!apg_v2_runtime_process_mono_ports(&runtime, "input", input, "output", output, 3u))
        return fail("named simple_gain processing failed");
    if (output[0] != 0.75f || output[1] != -1.5f || output[2] != 3.0f)
        return fail("unexpected named simple_gain output sample");

    if (apg_v2_runtime_process_mono_ports(&runtime, "missing", input, "output", output, 3u))
        return fail("named simple_gain accepted missing input port");
    const char *last_error = apg_v2_runtime_last_error(&runtime);
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
    status = apg_v2_runtime_init(&plan, 8u, 48000.0f, &runtime, &err);
    if (status != UC_OK) {
        fprintf(stderr, "runtime init error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("failed to initialize control port runtime");
    }

    if (!apg_v2_runtime_set_control_port(&runtime, "gain", 4.0f))
        return fail("failed to set matching control port");
    if (apg_v2_runtime_set_control_port(&runtime, "input", 2.0f) ||
        apg_v2_runtime_set_control_port(&runtime, "missing", 2.0f))
        return fail("accepted invalid control port update");

    const float input[2]  = {0.25f, -0.5f};
    float       output[2] = {0.0f, 0.0f};
    if (!apg_v2_runtime_process_mono_ports(&runtime, "input", input, "output", output, 2u))
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
    status = apg_v2_runtime_init(&plan, 8u, 48000.0f, &runtime, &err);
    if (status != UC_OK) {
        fprintf(stderr, "runtime init error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("failed to initialize stereo public port runtime");
    }

    const float input[2]  = {1.0f, 2.0f};
    float       output[2] = {0.0f, 0.0f};
    if (apg_v2_runtime_process_mono_ports(&runtime, "input", input, "output", output, 2u))
        return fail("named mono processing accepted a multi-channel input port");
    const char *last_error = apg_v2_runtime_last_error(&runtime);
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
    status = apg_v2_runtime_init(&plan, 8u, 48000.0f, &runtime, &err);
    if (status != UC_OK) {
        fprintf(stderr, "runtime init error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("failed to initialize delay state runtime");
    }
    if (runtime.nodes_len != 1u || runtime.nodes[0].state_buffers_len != 1u || !runtime.nodes[0].state_buffers[0])
        return fail("delay_line state buffer was not allocated");

    float *input  = apg_v2_runtime_find_signal(&runtime, "input");
    float *output = apg_v2_runtime_find_signal(&runtime, "output");
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

    apg_v2_runtime_destroy(&runtime);
    uc_arena_free(&arena);
    return 0;
}

static int test_runtime_init_failure_cleans_partial_allocations(void) {
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

    apg_v2_runtime_t runtime;
    uc_error         err    = {0};
    uc_status        status = apg_v2_runtime_init(&plan, 8u, 48000.0f, &runtime, &err);
    if (status == UC_OK)
        return fail("runtime init accepted node without atom metadata");
    if (runtime.signal_pool || runtime.signals || runtime.params || runtime.nodes || runtime.signals_len != 0u ||
        runtime.nodes_len != 0u)
        return fail("runtime init failure did not clean partial allocations");
    if (!strstr(err.msg, "atom metadata"))
        return fail("runtime init failure did not report useful error");
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
    if (test_delay_line_state_buffer_process())
        return 1;
    if (test_runtime_init_failure_cleans_partial_allocations())
        return 1;
    if (test_runtime_init_rejects_zero_capacity())
        return 1;
    return 0;
}
