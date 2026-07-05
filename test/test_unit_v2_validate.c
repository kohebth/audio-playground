#include <apgcore/unit_v2.h>

#include <stdio.h>
#include <string.h>

static int fail(const char *msg) {
    fprintf(stderr, "FAIL: %s\n", msg);
    return 1;
}

static int expect_valid_fixture(void) {
    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_unit_v2_t unit;
    uc_error      err    = {0};
    uc_status     status = apg_unit_v2_load_file("units-v2/simple_gain.unit.v2.yaml", &arena, &unit, &err);
    if (status != UC_OK) {
        fprintf(stderr, "validator error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("simple_gain v2 fixture did not validate");
    }

    if (strcmp(unit.name, "simple_gain") != 0)
        return fail("unexpected v2 unit name");
    if (strcmp(unit.version, "2.0.0") != 0)
        return fail("unexpected v2 unit version");
    if (!unit.meta.title || strcmp(unit.meta.title, "Simple Gain") != 0 || !unit.meta.category ||
        strcmp(unit.meta.category, "utility") != 0 || !unit.meta.description)
        return fail("unexpected parsed v2 unit meta");
    if (unit.params_len != 1u || unit.input_ports_len != 1u || unit.output_ports_len != 1u)
        return fail("unexpected v2 unit public surface counts");
    if (unit.signals_len != 3u || unit.nodes_len != 2u)
        return fail("unexpected v2 unit graph counts");
    if (unit.compatibility_len != 4u || strcmp(unit.compatibility[0].target, "desktop_full") != 0 ||
        strcmp(unit.compatibility[0].supported, "true") != 0)
        return fail("unexpected v2 unit compatibility flags");
    if (!unit.params || strcmp(unit.params[0].name, "gain") != 0 || strcmp(unit.params[0].type, "float") != 0)
        return fail("unexpected parsed v2 param metadata");
    if (strcmp(unit.params[0].default_value, "1.0") != 0 || strcmp(unit.params[0].min_value, "0.0") != 0 ||
        strcmp(unit.params[0].max_value, "4.0") != 0 || strcmp(unit.params[0].smoothing_ms, "10") != 0)
        return fail("unexpected parsed v2 param values");
    if (!unit.params[0].ui_label || strcmp(unit.params[0].ui_label, "Gain") != 0 || !unit.params[0].ui_control ||
        strcmp(unit.params[0].ui_control, "knob") != 0 || !unit.params[0].ui_unit ||
        strcmp(unit.params[0].ui_unit, "x") != 0)
        return fail("unexpected parsed v2 param ui metadata");
    if (!unit.input_ports || strcmp(unit.input_ports[0].name, "input") != 0 ||
        strcmp(unit.input_ports[0].type, "audio") != 0 || unit.input_ports[0].signals_len != 0u)
        return fail("unexpected parsed v2 input port");
    if (!unit.output_ports || strcmp(unit.output_ports[0].name, "output") != 0 ||
        strcmp(unit.output_ports[0].channels, "1") != 0)
        return fail("unexpected parsed v2 output port");
    if (!unit.signals || strcmp(unit.signals[0], "input") != 0 || strcmp(unit.signals[2], "gain_value") != 0)
        return fail("unexpected parsed v2 signal list");
    if (!unit.nodes || strcmp(unit.nodes[0].id, "gain_value") != 0 || strcmp(unit.nodes[0].atom, "generation_dc") != 0)
        return fail("unexpected parsed v2 first node");
    if (unit.nodes[0].config_len != 1u || strcmp(unit.nodes[0].config[0].key, "value") != 0 ||
        unit.nodes[0].config[0].value.kind != APG_V2_VALUE_VARREF ||
        strcmp(unit.nodes[0].config[0].value.text, "params.gain") != 0)
        return fail("unexpected parsed v2 config binding");
    if (strcmp(unit.nodes[1].id, "apply_gain") != 0 || unit.nodes[1].in_len != 2u || unit.nodes[1].out_len != 1u)
        return fail("unexpected parsed v2 second node bindings");

    uc_arena_free(&arena);
    return 0;
}

static int expect_invalid_contains(const char *yaml, const char *label, const char *must_contain) {
    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_unit_v2_t unit;
    uc_error      err    = {0};
    uc_status     status = apg_unit_v2_load_string(yaml, strlen(yaml), &arena, &unit, &err);
    uc_arena_free(&arena);

    if (status == UC_OK) {
        fprintf(stderr, "accepted invalid case: %s\n", label);
        return 1;
    }
    if (must_contain && !strstr(err.msg, must_contain)) {
        fprintf(stderr, "validation error for %s lacked '%s': %s\n", label, must_contain, err.msg);
        return 1;
    }
    return 0;
}

static int expect_invalid(const char *yaml, const char *label) { return expect_invalid_contains(yaml, label, NULL); }

static int expect_valid(const char *yaml, const char *label) {
    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_unit_v2_t unit;
    uc_error      err    = {0};
    uc_status     status = apg_unit_v2_load_string(yaml, strlen(yaml), &arena, &unit, &err);
    uc_arena_free(&arena);

    if (status != UC_OK) {
        fprintf(stderr, "rejected valid case %s: %s\n", label, err.msg);
        return 1;
    }
    return 0;
}

int main(void) {
    if (expect_valid_fixture())
        return 1;

    const char *bool_param = "kind: apg.unit\n"
                             "schema: apg.unit.v2\n"
                             "name: bool_param\n"
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
                             "    - id: pass\n"
                             "      atom: amplitude_multiply\n"
                             "compatibility:\n"
                             "  desktop_full: true\n";
    if (expect_valid(bool_param, "bool param without numeric bounds"))
        return 1;

    const char *all_known_profiles = "kind: apg.unit\n"
                                     "schema: apg.unit.v2\n"
                                     "name: known_profiles\n"
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
                                     "    - id: pass\n"
                                     "      atom: amplitude_multiply\n"
                                     "compatibility:\n"
                                     "  desktop_full: true\n"
                                     "  wasm_realtime: true\n"
                                     "  m7_static: false\n"
                                     "  offline_render: true\n";
    if (expect_valid(all_known_profiles, "all known compatibility profiles"))
        return 1;

    const char *meta_not_map = "kind: apg.unit\n"
                               "schema: apg.unit.v2\n"
                               "name: bad_unit\n"
                               "version: 2.0.0\n"
                               "meta: bad\n"
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
                               "    - id: pass\n"
                               "      atom: amplitude_multiply\n"
                               "compatibility:\n"
                               "  desktop_full: true\n";
    if (expect_invalid_contains(meta_not_map, "meta not map", "meta"))
        return 1;

    const char *unit_ui_not_map = "kind: apg.unit\n"
                                  "schema: apg.unit.v2\n"
                                  "name: bad_unit\n"
                                  "version: 2.0.0\n"
                                  "ui: compact\n"
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
                                  "    - id: pass\n"
                                  "      atom: amplitude_multiply\n"
                                  "compatibility:\n"
                                  "  desktop_full: true\n";
    if (expect_invalid_contains(unit_ui_not_map, "unit ui not map", "ui"))
        return 1;

    const char *param_ui_invalid_control = "kind: apg.unit\n"
                                           "schema: apg.unit.v2\n"
                                           "name: bad_unit\n"
                                           "version: 2.0.0\n"
                                           "params:\n"
                                           "  gain:\n"
                                           "    type: float\n"
                                           "    default: 1.0\n"
                                           "    min: 0.0\n"
                                           "    max: 2.0\n"
                                           "    ui:\n"
                                           "      control: wheel\n"
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
                                           "    - id: pass\n"
                                           "      atom: amplitude_multiply\n"
                                           "compatibility:\n"
                                           "  desktop_full: true\n";
    if (expect_invalid_contains(param_ui_invalid_control, "param ui invalid control", "params.gain.ui.control"))
        return 1;

    const char *param_ui_invalid_scale = "kind: apg.unit\n"
                                         "schema: apg.unit.v2\n"
                                         "name: bad_unit\n"
                                         "version: 2.0.0\n"
                                         "params:\n"
                                         "  gain:\n"
                                         "    type: float\n"
                                         "    default: 1.0\n"
                                         "    min: 0.0\n"
                                         "    max: 2.0\n"
                                         "    ui:\n"
                                         "      scale: stepped\n"
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
                                         "    - id: pass\n"
                                         "      atom: amplitude_multiply\n"
                                         "compatibility:\n"
                                         "  desktop_full: true\n";
    if (expect_invalid_contains(param_ui_invalid_scale, "param ui invalid scale", "params.gain.ui.scale"))
        return 1;

    const char *param_ui_invalid_precision = "kind: apg.unit\n"
                                             "schema: apg.unit.v2\n"
                                             "name: bad_unit\n"
                                             "version: 2.0.0\n"
                                             "params:\n"
                                             "  gain:\n"
                                             "    type: float\n"
                                             "    default: 1.0\n"
                                             "    min: 0.0\n"
                                             "    max: 2.0\n"
                                             "    ui:\n"
                                             "      display_precision: high\n"
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
                                             "    - id: pass\n"
                                             "      atom: amplitude_multiply\n"
                                             "compatibility:\n"
                                             "  desktop_full: true\n";
    if (expect_invalid_contains(param_ui_invalid_precision, "param ui invalid precision", "display_precision"))
        return 1;

    const char *unknown_param_type = "kind: apg.unit\n"
                                     "schema: apg.unit.v2\n"
                                     "name: bad_unit\n"
                                     "version: 2.0.0\n"
                                     "params:\n"
                                     "  label:\n"
                                     "    type: string\n"
                                     "    default: clean\n"
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
                                     "    - id: pass\n"
                                     "      atom: amplitude_multiply\n"
                                     "compatibility:\n"
                                     "  desktop_full: true\n";
    if (expect_invalid_contains(unknown_param_type, "unknown param type", "label"))
        return 1;

    const char *numeric_param_missing_bounds = "kind: apg.unit\n"
                                               "schema: apg.unit.v2\n"
                                               "name: bad_unit\n"
                                               "version: 2.0.0\n"
                                               "params:\n"
                                               "  gain:\n"
                                               "    type: float\n"
                                               "    default: 1.0\n"
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
                                               "    - id: pass\n"
                                               "      atom: amplitude_multiply\n"
                                               "compatibility:\n"
                                               "  desktop_full: true\n";
    if (expect_invalid_contains(numeric_param_missing_bounds, "numeric param missing bounds", "gain"))
        return 1;

    const char *duplicate_param = "kind: apg.unit\n"
                                  "schema: apg.unit.v2\n"
                                  "name: bad_unit\n"
                                  "version: 2.0.0\n"
                                  "params:\n"
                                  "  gain:\n"
                                  "    type: float\n"
                                  "    default: 1.0\n"
                                  "    min: 0.0\n"
                                  "    max: 2.0\n"
                                  "  gain:\n"
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
                                  "    - name: output\n"
                                  "      type: audio\n"
                                  "      channels: 1\n"
                                  "graph:\n"
                                  "  signals:\n"
                                  "    - input\n"
                                  "    - output\n"
                                  "  nodes:\n"
                                  "    - id: pass\n"
                                  "      atom: amplitude_multiply\n"
                                  "compatibility:\n"
                                  "  desktop_full: true\n";
    if (expect_invalid_contains(duplicate_param, "duplicate param name", "gain"))
        return 1;

    const char *missing_port_signal = "kind: apg.unit\n"
                                      "schema: apg.unit.v2\n"
                                      "name: bad_unit\n"
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
                                      "  nodes:\n"
                                      "    - id: gain_value\n"
                                      "      atom: generation_dc\n"
                                      "compatibility:\n"
                                      "  desktop_full: true\n";
    if (expect_invalid_contains(missing_port_signal, "missing matching output signal", "output"))
        return 1;

    const char *stereo_port_signals = "kind: apg.unit\n"
                                      "schema: apg.unit.v2\n"
                                      "name: stereo_ports\n"
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
                                      "        - left_in\n"
                                      "        - right_in\n"
                                      "  outputs:\n"
                                      "    - name: output\n"
                                      "      type: audio\n"
                                      "      channels: 2\n"
                                      "      signals:\n"
                                      "        - left_out\n"
                                      "        - right_out\n"
                                      "graph:\n"
                                      "  signals:\n"
                                      "    - left_in\n"
                                      "    - right_in\n"
                                      "    - left_out\n"
                                      "    - right_out\n"
                                      "  nodes:\n"
                                      "    - id: pass\n"
                                      "      atom: mix_decode_ms\n"
                                      "compatibility:\n"
                                      "  desktop_full: true\n";
    if (expect_valid(stereo_port_signals, "stereo port explicit signals"))
        return 1;

    const char *stereo_port_missing_signals = "kind: apg.unit\n"
                                              "schema: apg.unit.v2\n"
                                              "name: bad_unit\n"
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
                                              "  outputs:\n"
                                              "    - name: output\n"
                                              "      type: audio\n"
                                              "      channels: 1\n"
                                              "graph:\n"
                                              "  signals:\n"
                                              "    - output\n"
                                              "  nodes:\n"
                                              "    - id: pass\n"
                                              "      atom: generation_dc\n"
                                              "compatibility:\n"
                                              "  desktop_full: true\n";
    if (expect_invalid_contains(stereo_port_missing_signals, "multi-channel missing signals", "signals"))
        return 1;

    const char *stereo_port_signal_count = "kind: apg.unit\n"
                                           "schema: apg.unit.v2\n"
                                           "name: bad_unit\n"
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
                                           "  outputs:\n"
                                           "    - name: output\n"
                                           "      type: audio\n"
                                           "      channels: 1\n"
                                           "graph:\n"
                                           "  signals:\n"
                                           "    - input_l\n"
                                           "    - output\n"
                                           "  nodes:\n"
                                           "    - id: pass\n"
                                           "      atom: generation_dc\n"
                                           "compatibility:\n"
                                           "  desktop_full: true\n";
    if (expect_invalid_contains(stereo_port_signal_count, "stereo signal count", "count"))
        return 1;

    const char *control_port = "kind: apg.unit\n"
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
    if (expect_valid(control_port, "control port target map without channels or graph signal"))
        return 1;

    const char *legacy_control_port = "kind: apg.unit\n"
                                      "schema: apg.unit.v2\n"
                                      "name: legacy_control_port\n"
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
                                      "      target_param: gain\n"
                                      "  outputs:\n"
                                      "    - name: output\n"
                                      "      type: audio\n"
                                      "      channels: 1\n"
                                      "graph:\n"
                                      "  signals:\n"
                                      "    - input\n"
                                      "    - output\n"
                                      "  nodes:\n"
                                      "    - id: pass\n"
                                      "      atom: generation_dc\n"
                                      "compatibility:\n"
                                      "  desktop_full: true\n";
    if (expect_valid(legacy_control_port, "legacy control target_param"))
        return 1;

    const char *legacy_unknown_control_target = "kind: apg.unit\n"
                                                "schema: apg.unit.v2\n"
                                                "name: bad_unit\n"
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
                                                "    - name: amount\n"
                                                "      type: control\n"
                                                "      target_param: missing\n"
                                                "  outputs:\n"
                                                "    - name: output\n"
                                                "      type: audio\n"
                                                "      channels: 1\n"
                                                "graph:\n"
                                                "  signals:\n"
                                                "    - input\n"
                                                "    - output\n"
                                                "  nodes:\n"
                                                "    - id: pass\n"
                                                "      atom: generation_dc\n"
                                                "compatibility:\n"
                                                "  desktop_full: true\n";
    if (expect_invalid_contains(legacy_unknown_control_target, "legacy unknown control target", "target_param"))
        return 1;

    const char *unknown_control_target = "kind: apg.unit\n"
                                         "schema: apg.unit.v2\n"
                                         "name: bad_unit\n"
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
                                         "    - name: amount\n"
                                         "      type: control\n"
                                         "      target:\n"
                                         "        kind: param\n"
                                         "        name: missing\n"
                                         "  outputs:\n"
                                         "    - name: output\n"
                                         "      type: audio\n"
                                         "      channels: 1\n"
                                         "graph:\n"
                                         "  signals:\n"
                                         "    - input\n"
                                         "    - output\n"
                                         "  nodes:\n"
                                         "    - id: pass\n"
                                         "      atom: generation_dc\n"
                                         "compatibility:\n"
                                         "  desktop_full: true\n";
    if (expect_invalid_contains(unknown_control_target, "unknown control target", "target param"))
        return 1;

    const char *unsupported_control_target = "kind: apg.unit\n"
                                             "schema: apg.unit.v2\n"
                                             "name: bad_unit\n"
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
                                             "    - name: amount\n"
                                             "      type: control\n"
                                             "      target:\n"
                                             "        kind: graph_signal\n"
                                             "        name: input\n"
                                             "  outputs:\n"
                                             "    - name: output\n"
                                             "      type: audio\n"
                                             "      channels: 1\n"
                                             "graph:\n"
                                             "  signals:\n"
                                             "    - input\n"
                                             "    - output\n"
                                             "  nodes:\n"
                                             "    - id: pass\n"
                                             "      atom: generation_dc\n"
                                             "compatibility:\n"
                                             "  desktop_full: true\n";
    if (expect_invalid_contains(unsupported_control_target, "unsupported control target", "unsupported"))
        return 1;

    const char *ambiguous_control_target = "kind: apg.unit\n"
                                           "schema: apg.unit.v2\n"
                                           "name: bad_unit\n"
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
                                           "    - name: amount\n"
                                           "      type: control\n"
                                           "      target_param: gain\n"
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
                                           "  nodes:\n"
                                           "    - id: pass\n"
                                           "      atom: generation_dc\n"
                                           "compatibility:\n"
                                           "  desktop_full: true\n";
    if (expect_invalid_contains(ambiguous_control_target, "ambiguous control target", "both"))
        return 1;

    const char *audio_control_target = "kind: apg.unit\n"
                                       "schema: apg.unit.v2\n"
                                       "name: bad_unit\n"
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
                                       "      target_param: gain\n"
                                       "  outputs:\n"
                                       "    - name: output\n"
                                       "      type: audio\n"
                                       "      channels: 1\n"
                                       "graph:\n"
                                       "  signals:\n"
                                       "    - input\n"
                                       "    - output\n"
                                       "  nodes:\n"
                                       "    - id: pass\n"
                                       "      atom: generation_dc\n"
                                       "compatibility:\n"
                                       "  desktop_full: true\n";
    if (expect_invalid_contains(audio_control_target, "audio control target", "control target"))
        return 1;

    const char *unknown_port_type = "kind: apg.unit\n"
                                    "schema: apg.unit.v2\n"
                                    "name: bad_unit\n"
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
                                    "      type: event\n"
                                    "  outputs:\n"
                                    "    - name: output\n"
                                    "      type: audio\n"
                                    "      channels: 1\n"
                                    "graph:\n"
                                    "  signals:\n"
                                    "    - input\n"
                                    "    - output\n"
                                    "  nodes:\n"
                                    "    - id: bad\n"
                                    "      atom: generation_dc\n"
                                    "compatibility:\n"
                                    "  desktop_full: true\n";
    if (expect_invalid_contains(unknown_port_type, "unknown port type", "input"))
        return 1;

    const char *duplicate_port = "kind: apg.unit\n"
                                 "schema: apg.unit.v2\n"
                                 "name: bad_unit\n"
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
                                 "    - name: input\n"
                                 "      type: control\n"
                                 "  outputs:\n"
                                 "    - name: output\n"
                                 "      type: audio\n"
                                 "      channels: 1\n"
                                 "graph:\n"
                                 "  signals:\n"
                                 "    - input\n"
                                 "    - output\n"
                                 "  nodes:\n"
                                 "    - id: bad\n"
                                 "      atom: generation_dc\n"
                                 "compatibility:\n"
                                 "  desktop_full: true\n";
    if (expect_invalid_contains(duplicate_port, "duplicate port name", "input"))
        return 1;

    const char *duplicate_signal = "kind: apg.unit\n"
                                   "schema: apg.unit.v2\n"
                                   "name: bad_unit\n"
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
                                   "    - output\n"
                                   "  nodes:\n"
                                   "    - id: gain_value\n"
                                   "      atom: generation_dc\n"
                                   "compatibility:\n"
                                   "  desktop_full: true\n";
    if (expect_invalid_contains(duplicate_signal, "duplicate graph signal", "output"))
        return 1;

    const char *duplicate_binding_key = "kind: apg.unit\n"
                                        "schema: apg.unit.v2\n"
                                        "name: bad_unit\n"
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
                                        "        signal_a: gain_value\n"
                                        "      out:\n"
                                        "        signal: output\n"
                                        "compatibility:\n"
                                        "  desktop_full: true\n";
    if (expect_invalid_contains(duplicate_binding_key, "duplicate node binding key", "signal_a"))
        return 1;

    const char *unknown_atom = "kind: apg.unit\n"
                               "schema: apg.unit.v2\n"
                               "name: bad_unit\n"
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
                               "    - id: bad\n"
                               "      atom: not_an_atom\n"
                               "compatibility:\n"
                               "  desktop_full: true\n";
    if (expect_invalid_contains(unknown_atom, "unknown atom", "not_an_atom"))
        return 1;

    const char *unknown_param_ref = "kind: apg.unit\n"
                                    "schema: apg.unit.v2\n"
                                    "name: bad_unit\n"
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
                                    "        value: ${params.missing}\n"
                                    "compatibility:\n"
                                    "  desktop_full: true\n";
    if (expect_invalid(unknown_param_ref, "unknown param ref"))
        return 1;

    const char *compatibility_not_map = "kind: apg.unit\n"
                                        "schema: apg.unit.v2\n"
                                        "name: bad_unit\n"
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
                                        "    - id: pass\n"
                                        "      atom: amplitude_multiply\n"
                                        "compatibility: true\n";
    if (expect_invalid(compatibility_not_map, "compatibility not map"))
        return 1;

    const char *compatibility_non_bool = "kind: apg.unit\n"
                                         "schema: apg.unit.v2\n"
                                         "name: bad_unit\n"
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
                                         "    - id: pass\n"
                                         "      atom: amplitude_multiply\n"
                                         "compatibility:\n"
                                         "  desktop_full: maybe\n";
    if (expect_invalid(compatibility_non_bool, "compatibility non-bool flag"))
        return 1;

    const char *compatibility_unknown_profile = "kind: apg.unit\n"
                                                "schema: apg.unit.v2\n"
                                                "name: bad_unit\n"
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
                                                "    - id: pass\n"
                                                "      atom: amplitude_multiply\n"
                                                "compatibility:\n"
                                                "  toaster_realtime: true\n";
    if (expect_invalid_contains(compatibility_unknown_profile, "unknown compatibility profile", "toaster_realtime"))
        return 1;

    return 0;
}
