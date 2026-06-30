#include <apgcore/atom_catalog.h>
#include <apgcore/json_contract_v2.h>

#include <stdio.h>
#include <string.h>

static int usage(const char *argv0) {
    fprintf(
        stderr,
        "usage:\n"
        "  %s validate unit <path>\n"
        "  %s validate project <path>\n"
        "  %s inspect atoms\n"
        "  %s inspect unit <path>\n"
        "  %s inspect project <path>\n"
        "  %s render project <path>\n",
        argv0, argv0, argv0, argv0, argv0, argv0
    );
    return 2;
}

int main(int argc, char **argv) {
    if (argc < 3)
        return usage(argv[0]);

    if (strcmp(argv[1], "validate") == 0) {
        if (argc != 4)
            return usage(argv[0]);
        if (strcmp(argv[2], "unit") == 0) {
            apg_v2_json_write_validate_unit(stdout, argv[3]);
            fputc('\n', stdout);
            return 0;
        }
        if (strcmp(argv[2], "project") == 0) {
            apg_v2_json_write_validate_project(stdout, argv[3]);
            fputc('\n', stdout);
            return 0;
        }
        return usage(argv[0]);
    }

    if (strcmp(argv[1], "inspect") == 0) {
        if (strcmp(argv[2], "atoms") == 0) {
            if (argc != 3)
                return usage(argv[0]);
            apg_atom_catalog_write_json(stdout);
            return 0;
        }
        if (argc != 4)
            return usage(argv[0]);
        if (strcmp(argv[2], "unit") == 0) {
            apg_v2_json_write_inspect_unit(stdout, argv[3]);
            fputc('\n', stdout);
            return 0;
        }
        if (strcmp(argv[2], "project") == 0) {
            apg_v2_json_write_inspect_project(stdout, argv[3]);
            fputc('\n', stdout);
            return 0;
        }
        return usage(argv[0]);
    }

    if (strcmp(argv[1], "render") == 0) {
        if (argc != 4 || strcmp(argv[2], "project") != 0)
            return usage(argv[0]);
        apg_v2_json_write_render_project(stdout, argv[3]);
        fputc('\n', stdout);
        return 0;
    }

    return usage(argv[0]);
}
