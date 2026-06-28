#include <apgcore/compiler_v2.h>
#include <apgcore/unit_v2.h>

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
    uc_error      err = {0};
    uc_status status = apg_unit_v2_load_file("units-v2/simple_gain.unit.v2.yaml", &arena, &unit, &err);
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

static int expect_compile_invalid(const char *yaml, const char *label) {
    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_unit_v2_t unit;
    uc_error      err = {0};
    uc_status status = apg_unit_v2_load_string(yaml, strlen(yaml), &arena, &unit, &err);
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
    return 0;
}

static int expect_compile_valid(const char *yaml, const char *label) {
    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_unit_v2_t unit;
    uc_error      err = {0};
    uc_status status = apg_unit_v2_load_string(yaml, strlen(yaml), &arena, &unit, &err);
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
    const char *yaml =
        "kind: apg.unit\n"
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

    return expect_compile_invalid(yaml, "unknown signal binding");
}

static int test_unknown_binding_key_rejected(void) {
    const char *bad_input_key =
        "kind: apg.unit\n"
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
        "        siggnal_a: input\n"
        "        signal_b: gain_value\n"
        "      out:\n"
        "        signal: output\n"
        "compatibility:\n"
        "  desktop_full: true\n";
    if (expect_compile_invalid(bad_input_key, "unknown input binding key"))
        return 1;

    const char *bad_config_key =
        "kind: apg.unit\n"
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
        "        valuue: ${params.gain}\n"
        "compatibility:\n"
        "  desktop_full: true\n";
    return expect_compile_invalid(bad_config_key, "unknown config binding key");
}

static int test_control_ports_compile(void) {
    const char *yaml =
        "kind: apg.unit\n"
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
    const char *yaml =
        "kind: apg.unit\n"
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
    uc_error      err = {0};
    uc_status status = apg_unit_v2_load_string(yaml, strlen(yaml), &arena, &unit, &err);
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

    uc_arena_free(&arena);
    return 0;
}

static int test_signal_dependencies_rejected(void) {
    const char *unproduced_internal =
        "kind: apg.unit\n"
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

    const char *unproduced_output =
        "kind: apg.unit\n"
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
    if (test_control_ports_compile())
        return 1;
    if (test_forward_references_scheduled())
        return 1;
    if (test_signal_dependencies_rejected())
        return 1;
    return 0;
}
