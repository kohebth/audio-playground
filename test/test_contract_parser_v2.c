#include <apgcore/parser_v2.h>

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
    if (uc_arena_init(&arena, 4096) != 0)
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

int main(void) { return expect_parse_without_validation(); }
