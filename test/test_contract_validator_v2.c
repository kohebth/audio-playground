#include <apgcore/parser/parser_v2.h>
#include <apgcore/validator/project_validator_v2.h>
#include <apgcore/validator/unit_validator_v2.h>

#include <stdio.h>
#include <string.h>

static int fail(const char *msg) {
    fprintf(stderr, "FAIL: %s\n", msg);
    return 1;
}

static uc_status parse_doc(const char *yaml, uc_arena *arena, uc_node **root, uc_error *err) {
    return apg_v2_parse_string(yaml, strlen(yaml), arena, root, err);
}

static int expect_unit_validator_rejects_unknown_atom(void) {
    const char *yaml = "kind: apg.unit\n"
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
                       "    - id: missing_atom\n"
                       "      atom: not_registered\n"
                       "compatibility:\n"
                       "  desktop_full: true\n";

    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    uc_node *root = NULL;
    uc_error err  = {0};
    if (parse_doc(yaml, &arena, &root, &err) != UC_OK) {
        uc_arena_free(&arena);
        return fail("unit YAML did not parse");
    }

    apg_unit_v2_t unit   = {0};
    uc_status     status = apg_unit_v2_validate_root(root, &arena, &unit, &err);
    if (status == UC_OK || !strstr(err.msg, "unknown atom")) {
        fprintf(stderr, "validator error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("unit validator did not reject unknown atom");
    }

    uc_arena_free(&arena);
    return 0;
}

static int expect_project_validator_rejects_bad_route(void) {
    const char *yaml = "kind: apg.project\n"
                       "schema: apg.project.v2\n"
                       "name: bad_project\n"
                       "version: 2.0.0\n"
                       "units:\n"
                       "  - id: gain_unit\n"
                       "    file: ../units-v2/simple_gain.unit.v2.yaml\n"
                       "chain:\n"
                       "  nodes:\n"
                       "    - id: gain1\n"
                       "      unit: gain_unit\n"
                       "  routes:\n"
                       "    - from: missing.output\n"
                       "      to: gain1.input\n"
                       "targets:\n"
                       "  default: desktop_full\n";

    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    uc_node *root = NULL;
    uc_error err  = {0};
    if (parse_doc(yaml, &arena, &root, &err) != UC_OK) {
        uc_arena_free(&arena);
        return fail("project YAML did not parse");
    }

    apg_project_v2_t project = {0};
    uc_status        status  = apg_project_v2_validate_root(root, &arena, &project, &err);
    if (status == UC_OK || !strstr(err.msg, "chain.routes[].from")) {
        fprintf(stderr, "validator error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("project validator did not reject bad route");
    }

    uc_arena_free(&arena);
    return 0;
}

int main(void) {
    if (expect_unit_validator_rejects_unknown_atom())
        return 1;
    return expect_project_validator_rejects_bad_route();
}
