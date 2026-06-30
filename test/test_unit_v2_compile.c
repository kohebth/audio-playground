#include <apgcore/compiler_v2.h>
#include <apgcore/unit_v2.h>

#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int fail(const char *msg) {
    fprintf(stderr, "FAIL: %s\n", msg);
    return 1;
}

static int test_simple_gain_compile(void) {
    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_unit_v2_t unit;
    uc_error      err    = {0};
    uc_status     status = apg_unit_v2_load_file("units-v2/simple_gain.unit.v2.yaml", &arena, &unit, &err);
    if (status != UC_OK) {
        fprintf(stderr, "load error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("failed to load simple_gain fixture");
    }

    apg_v2_compiled_unit_t plan;
    status = apg_v2_compile_unit(&unit, &arena, &plan, &err);
    if (status != UC_OK) {
        fprintf(stderr, "compile error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("failed to compile simple_gain fixture");
    }

    if (plan.unit != &unit || plan.nodes_len != 2u || plan.schedule_len != 2u)
        return fail("unexpected compiled plan shape");
    if (plan.schedule[0] != 0u || plan.schedule[1] != 1u)
        return fail("unexpected compiled schedule");
    if (plan.signal_producers_len != 3u || plan.signal_producers[0] != UINT32_MAX || plan.signal_producers[1] != 1u ||
        plan.signal_producers[2] != 0u)
        return fail("unexpected simple_gain producer map");

    const apg_v2_compiled_node_t *dc = &plan.nodes[0];
    if (strcmp(dc->id, "gain_value") != 0 || !dc->atom || strcmp(dc->atom->name, "generation_dc") != 0)
        return fail("unexpected compiled dc node");
    if (dc->out_len != 1u || dc->out[0].kind != APG_BIND_SIGNAL || dc->out[0].index != 2u)
        return fail("unexpected compiled dc output binding");
    if (dc->config_len != 1u || dc->config[0].kind != APG_BIND_PARAM || dc->config[0].index != 0u)
        return fail("unexpected compiled dc config binding");

    const apg_v2_compiled_node_t *mul = &plan.nodes[1];
    if (strcmp(mul->id, "apply_gain") != 0 || !mul->atom || strcmp(mul->atom->name, "amplitude_multiply") != 0)
        return fail("unexpected compiled multiply node");
    if (mul->in_len != 2u || strcmp(mul->in[0].key, "signal_a") != 0 || mul->in[0].index != 0u)
        return fail("unexpected compiled multiply first input");
    if (strcmp(mul->in[1].key, "signal_b") != 0 || mul->in[1].index != 2u)
        return fail("unexpected compiled multiply second input");
    if (mul->out_len != 1u || strcmp(mul->out[0].key, "signal") != 0 || mul->out[0].index != 1u)
        return fail("unexpected compiled multiply output");

    uc_arena_free(&arena);
    return 0;
}

static int expect_compile_invalid_contains(
    const char *yaml,
    const char *label,
    const char *must_contain_a,
    const char *must_contain_b,
    const char *must_contain_c
) {
    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_unit_v2_t unit;
    uc_error      err    = {0};
    uc_status     status = apg_unit_v2_load_string(yaml, strlen(yaml), &arena, &unit, &err);
    if (status != UC_OK) {
        fprintf(stderr, "load error for %s: %s\n", label, err.msg);
        uc_arena_free(&arena);
        return fail("unexpected load failure before compile");
    }

    apg_v2_compiled_unit_t plan;
    status = apg_v2_compile_unit(&unit, &arena, &plan, &err);
    uc_arena_free(&arena);

    if (status == UC_OK) {
        fprintf(stderr, "accepted invalid compile case: %s\n", label);
        return 1;
    }
    if ((must_contain_a && !strstr(err.msg, must_contain_a)) || (must_contain_b && !strstr(err.msg, must_contain_b)) ||
        (must_contain_c && !strstr(err.msg, must_contain_c))) {
        fprintf(stderr, "compile error for %s lacked expected detail: %s\n", label, err.msg);
        return 1;
    }
    return 0;
}

static int expect_compile_invalid(const char *yaml, const char *label) {
    return expect_compile_invalid_contains(yaml, label, NULL, NULL, NULL);
}

static int expect_compile_valid(const char *yaml, const char *label) {
    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_unit_v2_t unit;
    uc_error      err    = {0};
    uc_status     status = apg_unit_v2_load_string(yaml, strlen(yaml), &arena, &unit, &err);
    if (status == UC_OK) {
        apg_v2_compiled_unit_t plan;
        status = apg_v2_compile_unit(&unit, &arena, &plan, &err);
    }
    uc_arena_free(&arena);

    if (status != UC_OK) {
        fprintf(stderr, "rejected valid compile case %s: %s\n", label, err.msg);
        return 1;
    }
    return 0;
}

static int test_unknown_signal_rejected(void) {
    const char *yaml = "kind: apg.unit\n"
                       "schema: apg.unit.v2\n"
                       "name: bad_compile\n"
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
                       "    - id: apply_gain\n"
                       "      atom: amplitude_multiply\n"
                       "      in:\n"
                       "        signal_a: input\n"
                       "        signal_b: missing_signal\n"
                       "      out:\n"
                       "        signal: output\n"
                       "compatibility:\n"
                       "  desktop_full: true\n";

    return expect_compile_invalid_contains(yaml, "unknown signal binding", "apply_gain", "signal_b", "missing_signal");
}

static int test_unknown_binding_key_rejected(void) {
    const char *bad_input_key = "kind: apg.unit\n"
                                "schema: apg.unit.v2\n"
                                "name: bad_key\n"
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
                                "      channels: 1\n"
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
                                "    - id: apply_gain\n"
                                "      atom: amplitude_multiply\n"
                                "      in:\n"
                                "        signal_a: input\n"
                                "        signal_b: gain_value\n"
                                "        siggnal_a: input\n"
                                "      out:\n"
                                "        signal: output\n"
                                "compatibility:\n"
                                "  desktop_full: true\n";
    if (expect_compile_invalid_contains(bad_input_key, "unknown input binding key", "apply_gain", "in", "siggnal_a"))
        return 1;

    const char *bad_config_key = "kind: apg.unit\n"
                                 "schema: apg.unit.v2\n"
                                 "name: bad_config\n"
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
                                 "      channels: 1\n"
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
                                 "        valuue: ${params.gain}\n"
                                 "compatibility:\n"
                                 "  desktop_full: true\n";
    return expect_compile_invalid_contains(
        bad_config_key, "unknown config binding key", "gain_value", "config", "valuue"
    );
}

static int test_required_bindings_rejected(void) {
    const char *missing_signal_b = "kind: apg.unit\n"
                                   "schema: apg.unit.v2\n"
                                   "name: missing_binding\n"
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
                                   "    - id: apply_gain\n"
                                   "      atom: amplitude_multiply\n"
                                   "      in:\n"
                                   "        signal_a: input\n"
                                   "      out:\n"
                                   "        signal: output\n"
                                   "compatibility:\n"
                                   "  desktop_full: true\n";
    if (expect_compile_invalid_contains(
            missing_signal_b, "missing amplitude_multiply signal_b", "apply_gain", "in", "signal_b"
        ))
        return 1;

    const char *missing_config_value = "kind: apg.unit\n"
                                       "schema: apg.unit.v2\n"
                                       "name: missing_config\n"
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
                                       "    - id: gain_value\n"
                                       "      atom: generation_dc\n"
                                       "      out:\n"
                                       "        signal: output\n"
                                       "compatibility:\n"
                                       "  desktop_full: true\n";
    return expect_compile_invalid_contains(
        missing_config_value, "missing generation_dc value config", "gain_value", "config", "value"
    );
}

static int test_extended_atom_binding_metadata_compile(void) {
    const char *amplitude_add = "kind: apg.unit\n"
                                "schema: apg.unit.v2\n"
                                "name: simple_add\n"
                                "version: 2.0.0\n"
                                "params:\n"
                                "  gain:\n"
                                "    type: float\n"
                                "    default: 1.0\n"
                                "    min: 0.0\n"
                                "    max: 2.0\n"
                                "ports:\n"
                                "  inputs:\n"
                                "    - name: a\n"
                                "      type: audio\n"
                                "      channels: 1\n"
                                "    - name: b\n"
                                "      type: audio\n"
                                "      channels: 1\n"
                                "  outputs:\n"
                                "    - name: output\n"
                                "      type: audio\n"
                                "      channels: 1\n"
                                "graph:\n"
                                "  signals:\n"
                                "    - a\n"
                                "    - b\n"
                                "    - output\n"
                                "  nodes:\n"
                                "    - id: add\n"
                                "      atom: amplitude_add\n"
                                "      in:\n"
                                "        signal_a: a\n"
                                "        signal_b: b\n"
                                "      out:\n"
                                "        signal: output\n"
                                "compatibility:\n"
                                "  desktop_full: true\n";
    if (expect_compile_valid(amplitude_add, "amplitude_add metadata"))
        return 1;

    const char *clip_soft = "kind: apg.unit\n"
                            "schema: apg.unit.v2\n"
                            "name: clip_soft\n"
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
                            "    - id: clip\n"
                            "      atom: amplitude_clip_soft\n"
                            "      in:\n"
                            "        signal: input\n"
                            "      out:\n"
                            "        signal: output\n"
                            "      config:\n"
                            "        threshold: 1.0\n"
                            "        curve: 0\n"
                            "compatibility:\n"
                            "  desktop_full: true\n";
    if (expect_compile_valid(clip_soft, "amplitude_clip_soft metadata"))
        return 1;

    const char *wet_dry = "kind: apg.unit\n"
                          "schema: apg.unit.v2\n"
                          "name: wet_dry\n"
                          "version: 2.0.0\n"
                          "params:\n"
                          "  mix:\n"
                          "    type: float\n"
                          "    default: 0.5\n"
                          "    min: 0.0\n"
                          "    max: 1.0\n"
                          "ports:\n"
                          "  inputs:\n"
                          "    - name: dry\n"
                          "      type: audio\n"
                          "      channels: 1\n"
                          "    - name: wet\n"
                          "      type: audio\n"
                          "      channels: 1\n"
                          "  outputs:\n"
                          "    - name: output\n"
                          "      type: audio\n"
                          "      channels: 1\n"
                          "graph:\n"
                          "  signals:\n"
                          "    - dry\n"
                          "    - wet\n"
                          "    - output\n"
                          "  nodes:\n"
                          "    - id: mix\n"
                          "      atom: mix_wet_dry\n"
                          "      in:\n"
                          "        dry: dry\n"
                          "        wet: wet\n"
                          "      out:\n"
                          "        signal: output\n"
                          "      config:\n"
                          "        mix: ${params.mix}\n"
                          "compatibility:\n"
                          "  desktop_full: true\n";
    if (expect_compile_valid(wet_dry, "mix_wet_dry metadata"))
        return 1;

    const char *lfo = "kind: apg.unit\n"
                      "schema: apg.unit.v2\n"
                      "name: lfo_fixture\n"
                      "version: 2.0.0\n"
                      "params:\n"
                      "  rate:\n"
                      "    type: float\n"
                      "    default: 5.0\n"
                      "    min: 0.1\n"
                      "    max: 12.0\n"
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
                      "    - id: lfo\n"
                      "      atom: generation_lfo\n"
                      "      out:\n"
                      "        signal: output\n"
                      "      config:\n"
                      "        frequency: ${params.rate}\n"
                      "        waveform: 0\n"
                      "        phase_offset: 0.0\n"
                      "        sample_rate: 0.0\n"
                      "compatibility:\n"
                      "  desktop_full: true\n";
    if (expect_compile_valid(lfo, "generation_lfo metadata"))
        return 1;

    const char *detect_threshold = "kind: apg.unit\n"
                                   "schema: apg.unit.v2\n"
                                   "name: threshold_fixture\n"
                                   "version: 2.0.0\n"
                                   "params:\n"
                                   "  threshold:\n"
                                   "    type: float\n"
                                   "    default: 0.2\n"
                                   "    min: 0.0\n"
                                   "    max: 1.0\n"
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
                                   "    - id: threshold\n"
                                   "      atom: detect_threshold\n"
                                   "      in:\n"
                                   "        signal: input\n"
                                   "      out:\n"
                                   "        gate: output\n"
                                   "      config:\n"
                                   "        threshold: ${params.threshold}\n"
                                   "compatibility:\n"
                                   "  desktop_full: true\n";
    return expect_compile_valid(detect_threshold, "detect_threshold metadata");
}

static int test_delay_line_binding_metadata_compile(void) {
    const char *delay_line = "kind: apg.unit\n"
                             "schema: apg.unit.v2\n"
                             "name: simple_delay\n"
                             "version: 2.0.0\n"
                             "params:\n"
                             "  delay_length:\n"
                             "    type: int\n"
                             "    default: 2\n"
                             "    min: 0\n"
                             "    max: 32\n"
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
                             "        length: ${params.delay_length}\n"
                             "compatibility:\n"
                             "  desktop_full: true\n";
    if (expect_compile_valid(delay_line, "delay_line metadata"))
        return 1;

    const char *missing_length = "kind: apg.unit\n"
                                 "schema: apg.unit.v2\n"
                                 "name: delay_missing_length\n"
                                 "version: 2.0.0\n"
                                 "params:\n"
                                 "  delay_length:\n"
                                 "    type: int\n"
                                 "    default: 2\n"
                                 "    min: 0\n"
                                 "    max: 32\n"
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
                                 "compatibility:\n"
                                 "  desktop_full: true\n";
    if (expect_compile_invalid_contains(missing_length, "missing delay_line length", "delay", "config", "length"))
        return 1;

    const char *bad_input_key = "kind: apg.unit\n"
                                "schema: apg.unit.v2\n"
                                "name: delay_bad_input_key\n"
                                "version: 2.0.0\n"
                                "params:\n"
                                "  delay_length:\n"
                                "    type: int\n"
                                "    default: 2\n"
                                "    min: 0\n"
                                "    max: 32\n"
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
                                "        siggnal: input\n"
                                "      out:\n"
                                "        signal: output\n"
                                "      config:\n"
                                "        length: ${params.delay_length}\n"
                                "compatibility:\n"
                                "  desktop_full: true\n";
    return expect_compile_invalid_contains(bad_input_key, "unknown delay_line input key", "delay", "in", "siggnal");
}

static int test_delay_family_binding_metadata_compile(void) {
    const char *delay_unit = "kind: apg.unit\n"
                             "schema: apg.unit.v2\n"
                             "name: unit_delay\n"
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
                             "    - id: unit_delay\n"
                             "      atom: delay_unit\n"
                             "      in:\n"
                             "        signal: input\n"
                             "      out:\n"
                             "        signal: output\n"
                             "compatibility:\n"
                             "  desktop_full: true\n";
    if (expect_compile_valid(delay_unit, "delay_unit metadata"))
        return 1;

    const char *bad_delay_unit_key = "kind: apg.unit\n"
                                     "schema: apg.unit.v2\n"
                                     "name: unit_delay_bad_key\n"
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
                                     "    - id: unit_delay\n"
                                     "      atom: delay_unit\n"
                                     "      in:\n"
                                     "        signal: input\n"
                                     "        sample: input\n"
                                     "      out:\n"
                                     "        signal: output\n"
                                     "compatibility:\n"
                                     "  desktop_full: true\n";
    if (expect_compile_invalid_contains(
            bad_delay_unit_key, "unknown delay_unit input key", "unit_delay", "in", "sample"
        ))
        return 1;

    const char *delay_fractional = "kind: apg.unit\n"
                                   "schema: apg.unit.v2\n"
                                   "name: fractional_delay\n"
                                   "version: 2.0.0\n"
                                   "params:\n"
                                   "  delay_samples:\n"
                                   "    type: float\n"
                                   "    default: 1.5\n"
                                   "    min: 0.0\n"
                                   "    max: 64.0\n"
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
                                   "    - id: fractional\n"
                                   "      atom: delay_fractional\n"
                                   "      in:\n"
                                   "        signal: input\n"
                                   "      out:\n"
                                   "        signal: output\n"
                                   "      config:\n"
                                   "        delay_samples: ${params.delay_samples}\n"
                                   "        interpolation: 0\n"
                                   "compatibility:\n"
                                   "  desktop_full: true\n";
    if (expect_compile_valid(delay_fractional, "delay_fractional metadata"))
        return 1;

    const char *missing_interpolation = "kind: apg.unit\n"
                                        "schema: apg.unit.v2\n"
                                        "name: fractional_delay_missing_config\n"
                                        "version: 2.0.0\n"
                                        "params:\n"
                                        "  delay_samples:\n"
                                        "    type: float\n"
                                        "    default: 1.5\n"
                                        "    min: 0.0\n"
                                        "    max: 64.0\n"
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
                                        "    - id: fractional\n"
                                        "      atom: delay_fractional\n"
                                        "      in:\n"
                                        "        signal: input\n"
                                        "      out:\n"
                                        "        signal: output\n"
                                        "      config:\n"
                                        "        delay_samples: ${params.delay_samples}\n"
                                        "compatibility:\n"
                                        "  desktop_full: true\n";
    if (expect_compile_invalid_contains(
            missing_interpolation, "missing delay_fractional interpolation", "fractional", "config", "interpolation"
        ))
        return 1;

    const char *delay_tap_feedback = "kind: apg.unit\n"
                                     "schema: apg.unit.v2\n"
                                     "name: delay_tap_feedback_valid\n"
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
                                     "    - id: tap\n"
                                     "      atom: delay_tap_feedback\n"
                                     "      in:\n"
                                     "        buffer: input\n"
                                     "        tap_position: 2\n"
                                     "      out:\n"
                                     "        signal: output\n"
                                     "      config:\n"
                                     "        coefficient: 0.5\n"
                                     "compatibility:\n"
                                     "  desktop_full: true\n";
    if (expect_compile_valid(delay_tap_feedback, "delay_tap_feedback metadata"))
        return 1;

    const char *delay_tap_feedforward = "kind: apg.unit\n"
                                        "schema: apg.unit.v2\n"
                                        "name: delay_tap_feedforward_valid\n"
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
                                        "      atom: delay_tap_feedforward\n"
                                        "      in:\n"
                                        "        buffer: input\n"
                                        "        tap_position: ${params.tap}\n"
                                        "      out:\n"
                                        "        signal: output\n"
                                        "      config:\n"
                                        "        coefficient: 0.5\n"
                                        "compatibility:\n"
                                        "  desktop_full: true\n";
    if (expect_compile_valid(delay_tap_feedforward, "delay_tap_feedforward metadata"))
        return 1;

    const char *missing_tap_position = "kind: apg.unit\n"
                                       "schema: apg.unit.v2\n"
                                       "name: delay_tap_missing_position\n"
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
                                       "    - id: tap\n"
                                       "      atom: delay_tap_feedback\n"
                                       "      in:\n"
                                       "        buffer: input\n"
                                       "      out:\n"
                                       "        signal: output\n"
                                       "      config:\n"
                                       "        coefficient: 0.5\n"
                                       "compatibility:\n"
                                       "  desktop_full: true\n";
    return expect_compile_invalid_contains(
        missing_tap_position, "missing delay_tap tap_position", "tap", "in", "tap_position"
    );
}

static int test_filter_binding_metadata_compile(void) {
    const char *filter_chain = "kind: apg.unit\n"
                               "schema: apg.unit.v2\n"
                               "name: filter_chain\n"
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
                               "    - biquad_out\n"
                               "    - allpass_out\n"
                               "    - comb_out\n"
                               "    - output\n"
                               "  nodes:\n"
                               "    - id: biquad\n"
                               "      atom: filter_biquad\n"
                               "      in:\n"
                               "        signal: input\n"
                               "      out:\n"
                               "        signal: biquad_out\n"
                               "      config:\n"
                               "        b0: 1.0\n"
                               "        b1: 0.0\n"
                               "        b2: 0.0\n"
                               "        a1: 0.0\n"
                               "        a2: 0.0\n"
                               "    - id: allpass\n"
                               "      atom: filter_allpass\n"
                               "      in:\n"
                               "        signal: biquad_out\n"
                               "      out:\n"
                               "        signal: allpass_out\n"
                               "      config:\n"
                               "        delay_samples: 4\n"
                               "        coefficient: 0.5\n"
                               "    - id: comb\n"
                               "      atom: filter_comb_ff\n"
                               "      in:\n"
                               "        signal: allpass_out\n"
                               "      out:\n"
                               "        signal: comb_out\n"
                               "      config:\n"
                               "        delay_samples: 8\n"
                               "        coefficient: 0.25\n"
                               "    - id: dc_block\n"
                               "      atom: filter_dc_block\n"
                               "      in:\n"
                               "        signal: comb_out\n"
                               "      out:\n"
                               "        signal: output\n"
                               "      config:\n"
                               "        coefficient: 0.995\n"
                               "compatibility:\n"
                               "  desktop_full: true\n";
    if (expect_compile_valid(filter_chain, "filter metadata chain"))
        return 1;

    const char *comb_fb_fallback_delay = "kind: apg.unit\n"
                                         "schema: apg.unit.v2\n"
                                         "name: comb_fb_fallback_delay\n"
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
                                         "    - id: comb_fb\n"
                                         "      atom: filter_comb_fb\n"
                                         "      in:\n"
                                         "        signal: input\n"
                                         "      out:\n"
                                         "        signal: output\n"
                                         "      config:\n"
                                         "        delay_samples: 4\n"
                                         "        coefficient: 0.5\n"
                                         "compatibility:\n"
                                         "  desktop_full: true\n";
    if (expect_compile_valid(comb_fb_fallback_delay, "filter_comb_fb fallback delay metadata"))
        return 1;

    const char *comb_fb_signal_delay = "kind: apg.unit\n"
                                       "schema: apg.unit.v2\n"
                                       "name: comb_fb_signal_delay\n"
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
                                       "    - name: delay\n"
                                       "      type: audio\n"
                                       "      channels: 1\n"
                                       "  outputs:\n"
                                       "    - name: output\n"
                                       "      type: audio\n"
                                       "      channels: 1\n"
                                       "graph:\n"
                                       "  signals:\n"
                                       "    - input\n"
                                       "    - delay\n"
                                       "    - output\n"
                                       "  nodes:\n"
                                       "    - id: comb_fb\n"
                                       "      atom: filter_comb_fb\n"
                                       "      in:\n"
                                       "        signal: input\n"
                                       "        delay: delay\n"
                                       "      out:\n"
                                       "        signal: output\n"
                                       "      config:\n"
                                       "        delay_samples: 4\n"
                                       "        coefficient: 0.5\n"
                                       "compatibility:\n"
                                       "  desktop_full: true\n";
    if (expect_compile_valid(comb_fb_signal_delay, "filter_comb_fb signal delay metadata"))
        return 1;

    const char *missing_comb_fb_signal = "kind: apg.unit\n"
                                         "schema: apg.unit.v2\n"
                                         "name: comb_fb_missing_signal\n"
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
                                         "    - id: comb_fb\n"
                                         "      atom: filter_comb_fb\n"
                                         "      out:\n"
                                         "        signal: output\n"
                                         "      config:\n"
                                         "        delay_samples: 4\n"
                                         "        coefficient: 0.5\n"
                                         "compatibility:\n"
                                         "  desktop_full: true\n";
    if (expect_compile_invalid_contains(
            missing_comb_fb_signal, "missing filter_comb_fb signal", "comb_fb", "in", "signal"
        ))
        return 1;

    const char *missing_b2 = "kind: apg.unit\n"
                             "schema: apg.unit.v2\n"
                             "name: filter_missing_b2\n"
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
                             "    - id: biquad\n"
                             "      atom: filter_biquad\n"
                             "      in:\n"
                             "        signal: input\n"
                             "      out:\n"
                             "        signal: output\n"
                             "      config:\n"
                             "        b0: 1.0\n"
                             "        b1: 0.0\n"
                             "        a1: 0.0\n"
                             "        a2: 0.0\n"
                             "compatibility:\n"
                             "  desktop_full: true\n";
    if (expect_compile_invalid_contains(missing_b2, "missing filter_biquad b2", "biquad", "config", "b2"))
        return 1;

    const char *bad_allpass_key = "kind: apg.unit\n"
                                  "schema: apg.unit.v2\n"
                                  "name: filter_bad_key\n"
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
                                  "    - id: allpass\n"
                                  "      atom: filter_allpass\n"
                                  "      in:\n"
                                  "        signal: input\n"
                                  "      out:\n"
                                  "        signal: output\n"
                                  "      config:\n"
                                  "        delay_samples: 4\n"
                                  "        coefficient: 0.5\n"
                                  "        coefficent: 0.25\n"
                                  "compatibility:\n"
                                  "  desktop_full: true\n";
    return expect_compile_invalid_contains(
        bad_allpass_key, "unknown filter_allpass config key", "allpass", "config", "coefficent"
    );
}

static int test_modulation_and_mix_binding_metadata_compile(void) {
    const char *modulation_chain = "kind: apg.unit\n"
                                   "schema: apg.unit.v2\n"
                                   "name: modulation_chain\n"
                                   "version: 2.0.0\n"
                                   "params:\n"
                                   "  depth:\n"
                                   "    type: float\n"
                                   "    default: 0.5\n"
                                   "    min: 0.0\n"
                                   "    max: 1.0\n"
                                   "ports:\n"
                                   "  inputs:\n"
                                   "    - name: input\n"
                                   "      type: audio\n"
                                   "      channels: 1\n"
                                   "    - name: modulator\n"
                                   "      type: audio\n"
                                   "      channels: 1\n"
                                   "    - name: position\n"
                                   "      type: audio\n"
                                   "      channels: 1\n"
                                   "  outputs:\n"
                                   "    - name: output\n"
                                   "      type: audio\n"
                                   "      channels: 1\n"
                                   "graph:\n"
                                   "  signals:\n"
                                   "    - input\n"
                                   "    - modulator\n"
                                   "    - position\n"
                                   "    - amp_out\n"
                                   "    - ring_out\n"
                                   "    - freq_out\n"
                                   "    - phase_out\n"
                                   "    - output\n"
                                   "  nodes:\n"
                                   "    - id: amp_mod\n"
                                   "      atom: modulation_amplitude\n"
                                   "      in:\n"
                                   "        signal: input\n"
                                   "        modulator: modulator\n"
                                   "      out:\n"
                                   "        signal: amp_out\n"
                                   "      config:\n"
                                   "        depth: ${params.depth}\n"
                                   "    - id: ring\n"
                                   "      atom: modulation_ring\n"
                                   "      in:\n"
                                   "        signal: amp_out\n"
                                   "        modulator: modulator\n"
                                   "      out:\n"
                                   "        signal: ring_out\n"
                                   "    - id: freq\n"
                                   "      atom: modulation_frequency\n"
                                   "      in:\n"
                                   "        signal: ring_out\n"
                                   "        modulator: modulator\n"
                                   "      out:\n"
                                   "        signal: freq_out\n"
                                   "      config:\n"
                                   "        depth: ${params.depth}\n"
                                   "    - id: phase\n"
                                   "      atom: modulation_phase\n"
                                   "      in:\n"
                                   "        signal: freq_out\n"
                                   "        modulator: modulator\n"
                                   "      out:\n"
                                   "        signal: phase_out\n"
                                   "      config:\n"
                                   "        depth: ${params.depth}\n"
                                   "    - id: scrub\n"
                                   "      atom: modulation_scrub\n"
                                   "      in:\n"
                                   "        buffer: phase_out\n"
                                   "        position: position\n"
                                   "      out:\n"
                                   "        signal: output\n"
                                   "      config:\n"
                                   "        buffer_size: 64\n"
                                   "compatibility:\n"
                                   "  desktop_full: true\n";
    if (expect_compile_valid(modulation_chain, "modulation metadata chain"))
        return 1;

    const char *mix_chain = "kind: apg.unit\n"
                            "schema: apg.unit.v2\n"
                            "name: mix_chain\n"
                            "version: 2.0.0\n"
                            "params:\n"
                            "  pan:\n"
                            "    type: float\n"
                            "    default: 0.5\n"
                            "    min: 0.0\n"
                            "    max: 1.0\n"
                            "ports:\n"
                            "  inputs:\n"
                            "    - name: a\n"
                            "      type: audio\n"
                            "      channels: 1\n"
                            "    - name: b\n"
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
                            "    - a\n"
                            "    - b\n"
                            "    - cross\n"
                            "    - pan_left\n"
                            "    - pan_right\n"
                            "    - mid\n"
                            "    - side\n"
                            "    - left\n"
                            "    - right\n"
                            "  nodes:\n"
                            "    - id: crossfade\n"
                            "      atom: mix_crossfade\n"
                            "      in:\n"
                            "        signal_a: a\n"
                            "        signal_b: b\n"
                            "      out:\n"
                            "        signal: cross\n"
                            "      config:\n"
                            "        t: 0.25\n"
                            "    - id: pan\n"
                            "      atom: mix_pan_stereo\n"
                            "      in:\n"
                            "        signal: cross\n"
                            "      out:\n"
                            "        left: pan_left\n"
                            "        right: pan_right\n"
                            "      config:\n"
                            "        position: ${params.pan}\n"
                            "    - id: encode\n"
                            "      atom: mix_encode_ms\n"
                            "      in:\n"
                            "        left: pan_left\n"
                            "        right: pan_right\n"
                            "      out:\n"
                            "        mid: mid\n"
                            "        side: side\n"
                            "    - id: decode\n"
                            "      atom: mix_decode_ms\n"
                            "      in:\n"
                            "        mid: mid\n"
                            "        side: side\n"
                            "      out:\n"
                            "        left: left\n"
                            "        right: right\n"
                            "compatibility:\n"
                            "  desktop_full: true\n";
    if (expect_compile_valid(mix_chain, "mix metadata chain"))
        return 1;

    const char *matrix_mix = "kind: apg.unit\n"
                             "schema: apg.unit.v2\n"
                             "name: matrix_mix\n"
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
                             "    - name: left\n"
                             "      type: audio\n"
                             "      channels: 1\n"
                             "    - name: right\n"
                             "      type: audio\n"
                             "      channels: 1\n"
                             "graph:\n"
                             "  signals:\n"
                             "    - a\n"
                             "    - b\n"
                             "    - left\n"
                             "    - right\n"
                             "  nodes:\n"
                             "    - id: matrix\n"
                             "      atom: mix_matrix\n"
                             "      in:\n"
                             "        signals:\n"
                             "          - a\n"
                             "          - b\n"
                             "      out:\n"
                             "        signals:\n"
                             "          - left\n"
                             "          - right\n"
                             "      config:\n"
                             "        coefficients:\n"
                             "          row0: { c0: 0.5, c1: 0.5 }\n"
                             "          row1: { c0: 1.0, c1: -1.0 }\n"
                             "compatibility:\n"
                             "  desktop_full: true\n";
    if (expect_compile_valid(matrix_mix, "mix_matrix metadata"))
        return 1;

    const char *bad_matrix_shape = "kind: apg.unit\n"
                                   "schema: apg.unit.v2\n"
                                   "name: matrix_mix_bad_shape\n"
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
                                   "    - name: output\n"
                                   "      type: audio\n"
                                   "      channels: 1\n"
                                   "graph:\n"
                                   "  signals:\n"
                                   "    - a\n"
                                   "    - b\n"
                                   "    - output\n"
                                   "  nodes:\n"
                                   "    - id: matrix\n"
                                   "      atom: mix_matrix\n"
                                   "      in:\n"
                                   "        signals:\n"
                                   "          - a\n"
                                   "          - b\n"
                                   "      out:\n"
                                   "        signals:\n"
                                   "          - output\n"
                                   "      config:\n"
                                   "        coefficients:\n"
                                   "          row0: { c0: 1.0 }\n"
                                   "compatibility:\n"
                                   "  desktop_full: true\n";
    if (expect_compile_invalid_contains(bad_matrix_shape, "bad mix_matrix shape", "matrix", "shape", "signal counts"))
        return 1;

    const char *missing_depth = "kind: apg.unit\n"
                                "schema: apg.unit.v2\n"
                                "name: modulation_missing_depth\n"
                                "version: 2.0.0\n"
                                "params:\n"
                                "  depth:\n"
                                "    type: float\n"
                                "    default: 0.5\n"
                                "    min: 0.0\n"
                                "    max: 1.0\n"
                                "ports:\n"
                                "  inputs:\n"
                                "    - name: input\n"
                                "      type: audio\n"
                                "      channels: 1\n"
                                "    - name: modulator\n"
                                "      type: audio\n"
                                "      channels: 1\n"
                                "  outputs:\n"
                                "    - name: output\n"
                                "      type: audio\n"
                                "      channels: 1\n"
                                "graph:\n"
                                "  signals:\n"
                                "    - input\n"
                                "    - modulator\n"
                                "    - output\n"
                                "  nodes:\n"
                                "    - id: amp_mod\n"
                                "      atom: modulation_amplitude\n"
                                "      in:\n"
                                "        signal: input\n"
                                "        modulator: modulator\n"
                                "      out:\n"
                                "        signal: output\n"
                                "compatibility:\n"
                                "  desktop_full: true\n";
    if (expect_compile_invalid_contains(missing_depth, "missing modulation depth", "amp_mod", "config", "depth"))
        return 1;

    const char *bad_pan_key = "kind: apg.unit\n"
                              "schema: apg.unit.v2\n"
                              "name: bad_pan_key\n"
                              "version: 2.0.0\n"
                              "params:\n"
                              "  pan:\n"
                              "    type: float\n"
                              "    default: 0.5\n"
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
                              "        pos: 0.5\n"
                              "compatibility:\n"
                              "  desktop_full: true\n";
    return expect_compile_invalid_contains(bad_pan_key, "unknown mix_pan_stereo config key", "pan", "config", "pos");
}

static int test_control_ports_compile(void) {
    const char *yaml = "kind: apg.unit\n"
                       "schema: apg.unit.v2\n"
                       "name: control_port\n"
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
                       "      channels: 1\n"
                       "    - name: bypass\n"
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

    return expect_compile_valid(yaml, "control port without signal");
}

static int test_forward_references_scheduled(void) {
    const char *yaml = "kind: apg.unit\n"
                       "schema: apg.unit.v2\n"
                       "name: forward_ref\n"
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
                       "      channels: 1\n"
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
                       "    - id: apply_gain\n"
                       "      atom: amplitude_multiply\n"
                       "      in:\n"
                       "        signal_a: input\n"
                       "        signal_b: gain_value\n"
                       "      out:\n"
                       "        signal: output\n"
                       "    - id: gain_value\n"
                       "      atom: generation_dc\n"
                       "      out:\n"
                       "        signal: gain_value\n"
                       "      config:\n"
                       "        value: ${params.gain}\n"
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
        return fail("failed to load forward reference unit");
    }

    apg_v2_compiled_unit_t plan;
    status = apg_v2_compile_unit(&unit, &arena, &plan, &err);
    if (status != UC_OK) {
        fprintf(stderr, "compile error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("failed to compile forward reference unit");
    }

    if (plan.schedule_len != 2u || plan.schedule[0] != 1u || plan.schedule[1] != 0u) {
        uc_arena_free(&arena);
        return fail("unexpected topological schedule for forward reference unit");
    }
    if (plan.signal_producers_len != 3u || plan.signal_producers[0] != UINT32_MAX || plan.signal_producers[1] != 0u ||
        plan.signal_producers[2] != 1u) {
        uc_arena_free(&arena);
        return fail("unexpected producer map for forward reference unit");
    }

    uc_arena_free(&arena);
    return 0;
}

static int has_unit_v2_yaml_suffix(const char *name) {
    size_t len    = name ? strlen(name) : 0u;
    size_t suffix = strlen(".unit.v2.yaml");
    return len >= suffix && strcmp(name + len - suffix, ".unit.v2.yaml") == 0;
}

static int test_compile_all_unit_v2_fixtures(void) {
    DIR *dir = opendir("units-v2");
    if (!dir)
        return fail("failed to open units-v2 fixture directory");

    size_t fixture_count = 0;
    for (struct dirent *entry = readdir(dir); entry; entry = readdir(dir)) {
        if (!has_unit_v2_yaml_suffix(entry->d_name))
            continue;

        char path[256];
        snprintf(path, sizeof(path), "units-v2/%s", entry->d_name);

        uc_arena arena;
        if (uc_arena_init(&arena, 1024 * 1024) != 0) {
            closedir(dir);
            return fail("arena init failed");
        }

        apg_unit_v2_t unit;
        uc_error      err    = {0};
        uc_status     status = apg_unit_v2_load_file(path, &arena, &unit, &err);
        if (status == UC_OK) {
            apg_v2_compiled_unit_t plan;
            status = apg_v2_compile_unit(&unit, &arena, &plan, &err);
        }
        uc_arena_free(&arena);

        if (status != UC_OK) {
            fprintf(stderr, "failed fixture %s: %s\n", path, err.msg);
            closedir(dir);
            return fail("failed to load and compile unit-v2 fixture");
        }
        fixture_count++;
    }
    closedir(dir);

    if (fixture_count < 14u)
        return fail("expected at least fourteen unit-v2 fixtures");
    return 0;
}

static int test_signal_dependencies_rejected(void) {
    const char *unproduced_internal = "kind: apg.unit\n"
                                      "schema: apg.unit.v2\n"
                                      "name: bad_dependency\n"
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
                                      "      channels: 1\n"
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
                                      "    - id: apply_gain\n"
                                      "      atom: amplitude_multiply\n"
                                      "      in:\n"
                                      "        signal_a: input\n"
                                      "        signal_b: gain_value\n"
                                      "      out:\n"
                                      "        signal: output\n"
                                      "compatibility:\n"
                                      "  desktop_full: true\n";
    if (expect_compile_invalid(unproduced_internal, "unproduced internal signal"))
        return 1;

    const char *unproduced_output = "kind: apg.unit\n"
                                    "schema: apg.unit.v2\n"
                                    "name: bad_output\n"
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
                                    "      channels: 1\n"
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
                                    "compatibility:\n"
                                    "  desktop_full: true\n";
    return expect_compile_invalid(unproduced_output, "unproduced output signal");
}

int main(void) {
    if (test_simple_gain_compile())
        return 1;
    if (test_unknown_signal_rejected())
        return 1;
    if (test_unknown_binding_key_rejected())
        return 1;
    if (test_required_bindings_rejected())
        return 1;
    if (test_extended_atom_binding_metadata_compile())
        return 1;
    if (test_delay_line_binding_metadata_compile())
        return 1;
    if (test_delay_family_binding_metadata_compile())
        return 1;
    if (test_filter_binding_metadata_compile())
        return 1;
    if (test_modulation_and_mix_binding_metadata_compile())
        return 1;
    if (test_control_ports_compile())
        return 1;
    if (test_forward_references_scheduled())
        return 1;
    if (test_signal_dependencies_rejected())
        return 1;
    if (test_compile_all_unit_v2_fixtures())
        return 1;
    return 0;
}
