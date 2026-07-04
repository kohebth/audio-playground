#include <apgcore/parser_v2.h>
#include <apgcore/project_v2.h>
#include <apgcore/project_validator_v2.h>
#include <apgcore/unit_v2.h>
#include <apgcore/unit_validator_v2.h>

#include <stdio.h>
#include <string.h>

static int fail(const char *msg) {
    fprintf(stderr, "FAIL: %s\n", msg);
    return 1;
}

static int expect_parse_without_validation(void) {
    const char *yaml = "kind: apg.unit\n"
                       "schema: apg.unit.v2\n"
                       "name: raw_contract\n"
                       "version: 2.0.0\n"
                       "graph:\n"
                       "  nodes:\n"
                       "    - id: unknown\n"
                       "      atom: not_registered\n";

    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    uc_node  *root   = NULL;
    uc_error  err    = {0};
    uc_status status = apg_v2_parse_string(yaml, strlen(yaml), &arena, &root, &err);
    if (status != UC_OK || !root || root->kind != UC_NODE_MAP) {
        fprintf(stderr, "parser error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("valid YAML did not parse");
    }

    const uc_node *name = uc_node_find(root, "name");
    if (!name || name->kind != UC_NODE_SCALAR || strcmp(name->text, "raw_contract") != 0) {
        uc_arena_free(&arena);
        return fail("parsed graph did not preserve scalar fields");
    }

    uc_arena_free(&arena);
    return 0;
}

static int expect_boundary_parse_then_validate(void) {
    const char *unit_file    = "units-v2/simple_gain.unit.v2.yaml";
    const char *project_file = "projects-v2/simple-gain-board.project.v2.yaml";

    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    uc_node  *unit_root = NULL;
    uc_error  err       = {0};
    uc_status status    = apg_unit_v2_parse_file(unit_file, &arena, &unit_root, &err);
    if (status != UC_OK || !unit_root) {
        fprintf(stderr, "unit parse error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("unit parser wrapper failed");
    }
    apg_unit_v2_t unit = {0};
    status             = apg_unit_v2_validate_root(unit_root, &arena, &unit, &err);
    if (status != UC_OK || unit.name == NULL || strcmp(unit.name, "simple_gain") != 0) {
        fprintf(stderr, "unit validation error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("unit parser boundary validation failed");
    }

    uc_node *project_root = NULL;
    err                   = (uc_error){0};
    status                = apg_project_v2_parse_file(project_file, &arena, &project_root, &err);
    if (status != UC_OK || !project_root) {
        fprintf(stderr, "project parse error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("project parser wrapper failed");
    }
    apg_project_v2_t project = {0};
    status                   = apg_project_v2_validate_root(project_root, &arena, &project, &err);
    if (status != UC_OK || !project.name || strcmp(project.name, "simple-gain-board") != 0) {
        fprintf(stderr, "project validation error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("project parser boundary validation failed");
    }

    uc_arena_free(&arena);
    return 0;
}

static int expect_parse_only_without_semantic_checks(void) {
    const char *yaml = "kind: apg.unit\n"
                       "version: 2.0.0\n"
                       "name: boundary_contract\n"
                       "graph:\n"
                       "  signals:\n"
                       "    - in\n"
                       "    - out\n"
                       "  nodes: []\n"
                       "compatibility:\n"
                       "  desktop_full: true\n"
                       "  m7_static: true\n";

    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    uc_node  *root = NULL;
    uc_error  err  = {0};
    uc_status status = apg_unit_v2_parse_string(yaml, strlen(yaml), &arena, &root, &err);
    if (status != UC_OK || !root) {
        fprintf(stderr, "parser error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("semantic-invalid unit contract should still parse as raw contract");
    }

    apg_unit_v2_t unit = {0};
    status             = apg_unit_v2_validate_root(root, &arena, &unit, &err);
    if (status == UC_OK) {
        uc_arena_free(&arena);
        return fail("validate should fail when semantic requirements are missing");
    }

    uc_arena_free(&arena);
    return 0;
}

static int expect_project_parse_only_without_semantic_checks(void) {
    const char *yaml = "kind: apg.project\n"
                       "schema: apg.project.v2\n"
                       "name: project_boundary\n"
                       "version: 2.0.0\n"
                       "units:\n"
                       "  - id: unit_a\n"
                       "    file: ../units-v2/simple_gain.unit.v2.yaml\n"
                       "chain:\n"
                       "  nodes:\n"
                       "    - id: node_missing\n"
                       "      unit: unknown\n"
                       "  routes:\n"
                       "    - from: system.input\n"
                       "      to: node_missing.input\n"
                       "targets:\n"
                       "  default: desktop_full\n";

    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    uc_node  *root = NULL;
    uc_error  err  = {0};
    uc_status status = apg_project_v2_parse_string(yaml, strlen(yaml), &arena, &root, &err);
    if (status != UC_OK || !root) {
        fprintf(stderr, "project parser error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("semantic-invalid project contract should still parse as raw contract");
    }

    apg_project_v2_t project = {0};
    status                  = apg_project_v2_validate_root(root, &arena, &project, &err);
    if (status == UC_OK) {
        uc_arena_free(&arena);
        return fail("project validator should reject unknown unit references");
    }

    uc_arena_free(&arena);
    return 0;
}

int main(void) {
    int status = expect_parse_without_validation();
    if (status != 0)
        return status;
    status = expect_boundary_parse_then_validate();
    if (status != 0)
        return status;
    status = expect_parse_only_without_semantic_checks();
    if (status != 0)
        return status;
    return expect_project_parse_only_without_semantic_checks();
}
