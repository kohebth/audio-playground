#include <apgcore/atom_catalog.h>
#include <apgcore/json_contract_v2.h>
#include <apgcore/project_compiler_v2.h>
#include <apgcore/project_v2.h>

#include <stdbool.h>
#include <stdint.h>
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
        "  %s render project <path>\n"
        "  %s benchmark project <path>\n"
        "  %s export --target <wasm_realtime|m7_static> <project> <outdir>\n",
        argv0, argv0, argv0, argv0, argv0, argv0, argv0, argv0
    );
    return 2;
}

static void write_json_string(FILE *out, const char *text) {
    fputc('"', out);
    if (text) {
        for (const unsigned char *p = (const unsigned char *)text; *p; p++) {
            if (*p == '"' || *p == '\\')
                fputc('\\', out);
            if (*p == '\n')
                fputs("\\n", out);
            else if (*p >= 0x20)
                fputc(*p, out);
        }
    }
    fputc('"', out);
}

static void write_c_string(FILE *out, const char *text) {
    fputc('"', out);
    if (text) {
        for (const unsigned char *p = (const unsigned char *)text; *p; p++) {
            if (*p == '"' || *p == '\\')
                fputc('\\', out);
            if (*p == '\n')
                fputs("\\n", out);
            else if (*p >= 0x20 && *p < 0x7f)
                fputc(*p, out);
            else
                fprintf(out, "\\x%02x", (unsigned)*p);
        }
    }
    fputc('"', out);
}

static const char *status_code(uc_status status) {
    switch (status) {
    case UC_OK:
        return "APG_OK";
    case UC_E_IO:
        return "APG_IO_ERROR";
    case UC_E_OOM:
        return "APG_OUT_OF_MEMORY";
    case UC_E_LEX:
        return "APG_LEX_ERROR";
    case UC_E_PARSE:
        return "APG_PARSE_ERROR";
    case UC_E_TYPE:
        return "APG_TYPE_ERROR";
    case UC_E_RANGE:
        return "APG_RANGE_ERROR";
    case UC_E_MISSING:
        return "APG_MISSING_ERROR";
    }
    return "APG_ERROR";
}

static int write_cli_error(FILE *out, const char *schema, const char *file, const char *target, const uc_error *err) {
    fputs("{\"schema\":", out);
    write_json_string(out, schema);
    fputs(",\"ok\":false,\"file\":", out);
    write_json_string(out, file);
    if (target) {
        fputs(",\"target\":", out);
        write_json_string(out, target);
    }
    fputs(",\"diagnostics\":[{\"code\":", out);
    write_json_string(out, status_code(err ? err->status : UC_E_TYPE));
    fputs(",\"message\":", out);
    write_json_string(out, err && err->msg[0] ? err->msg : "operation failed");
    fputs("}]}\n", out);
    return 1;
}

static uc_status load_compile_project(
    const char                *path,
    uc_arena                  *arena,
    apg_project_v2_resolved_t *project,
    apg_project_v2_compiled_t *compiled,
    uc_error                  *err
) {
    uc_status status = apg_project_v2_load_resolved_file(path, arena, project, err);
    if (status == UC_OK)
        status = apg_project_v2_compile(project, arena, compiled, err);
    return status;
}

static int benchmark_project(const char *path) {
    uc_arena arena;
    if (uc_arena_init(&arena, 2 * 1024 * 1024) != 0) {
        uc_error err = {.status = UC_E_OOM};
        return write_cli_error(stdout, "apg.project.benchmark.v1", path, NULL, &err);
    }

    apg_project_v2_resolved_t project;
    apg_project_v2_compiled_t compiled;
    uc_error                  err    = {0};
    uc_status                 status = load_compile_project(path, &arena, &project, &compiled, &err);
    if (status != UC_OK) {
        int rc = write_cli_error(stdout, "apg.project.benchmark.v1", path, NULL, &err);
        uc_arena_free(&arena);
        return rc;
    }

    fputs("{\"schema\":\"apg.project.benchmark.v1\",\"ok\":true,\"file\":", stdout);
    write_json_string(stdout, path);
    fprintf(
        stdout,
        ",\"sample_rate\":48000,\"block_frames\":64,\"iterations\":0,"
        "\"structural\":{\"units\":%zu,\"project_nodes\":%zu,\"params\":%zu,\"signals\":%zu,\"nodes\":%zu,"
        "\"schedule\":%zu},\"timing\":{\"available\":false}}\n",
        project.units_len, project.project.nodes_len, compiled.expanded_unit.params_len,
        compiled.expanded_unit.signals_len, compiled.plan.nodes_len, compiled.plan.schedule_len
    );
    uc_arena_free(&arena);
    return 0;
}

static bool unit_supports_target(const apg_unit_v2_t *unit, const char *target) {
    for (size_t i = 0; unit && i < unit->compatibility_len; i++) {
        if (unit->compatibility[i].target && strcmp(unit->compatibility[i].target, target) == 0)
            return unit->compatibility[i].supported && strcmp(unit->compatibility[i].supported, "true") == 0;
    }
    return false;
}

static const apg_project_v2_loaded_unit_t *
first_unsupported_unit(const apg_project_v2_resolved_t *project, const char *target) {
    for (size_t i = 0; i < project->units_len; i++) {
        if (!unit_supports_target(&project->units[i].unit, target))
            return &project->units[i];
    }
    return NULL;
}

static bool join_path(char *out, size_t out_size, const char *dir, const char *name) {
    size_t dir_len = dir ? strlen(dir) : 0u;
    int    written =
        snprintf(out, out_size, "%s%s%s", dir ? dir : "", dir_len > 0u && dir[dir_len - 1u] == '/' ? "" : "/", name);
    return written >= 0 && (size_t)written < out_size;
}

static bool write_m7_header(const char *path, const apg_project_v2_compiled_t *compiled) {
    FILE *out = fopen(path, "w");
    if (!out)
        return false;
    fputs("#ifndef APG_PROJECT_M7_BUNDLE_H\n#define APG_PROJECT_M7_BUNDLE_H\n\n", out);
    fputs("#include <stddef.h>\n#include <stdint.h>\n\n", out);
    fprintf(out, "#define APG_M7_PROJECT_PARAM_COUNT %zuu\n", compiled->expanded_unit.params_len);
    fprintf(out, "#define APG_M7_PROJECT_SIGNAL_COUNT %zuu\n", compiled->expanded_unit.signals_len);
    fprintf(out, "#define APG_M7_PROJECT_NODE_COUNT %zuu\n", compiled->plan.nodes_len);
    fprintf(out, "#define APG_M7_PROJECT_SCHEDULE_COUNT %zuu\n\n", compiled->plan.schedule_len);
    fputs("#define APG_M7_PROJECT_USES_RUNTIME_YAML 0u\n", out);
    fputs("#define APG_M7_PROJECT_USES_DYNAMIC_ALLOCATION 0u\n\n", out);
    fputs("extern const char apg_m7_project_name[];\n", out);
    fputs("extern const uint32_t apg_m7_project_schedule[APG_M7_PROJECT_SCHEDULE_COUNT];\n", out);
    fputs("extern const char *const apg_m7_project_nodes[APG_M7_PROJECT_NODE_COUNT];\n", out);
    fputs("\n#endif\n", out);
    return fclose(out) == 0;
}

static bool
write_m7_source(const char *path, const apg_project_v2_resolved_t *project, const apg_project_v2_compiled_t *compiled) {
    FILE *out = fopen(path, "w");
    if (!out)
        return false;
    fputs("#include \"apg_project_m7.h\"\n\n", out);
    fputs("const char apg_m7_project_name[] = ", out);
    write_c_string(out, project->project.name);
    fputs(";\n\nconst uint32_t apg_m7_project_schedule[APG_M7_PROJECT_SCHEDULE_COUNT] = {", out);
    for (size_t i = 0; i < compiled->plan.schedule_len; i++) {
        if (i > 0u)
            fputs(", ", out);
        fprintf(out, "%uu", (unsigned)compiled->plan.schedule[i]);
    }
    fputs("};\n\nconst char *const apg_m7_project_nodes[APG_M7_PROJECT_NODE_COUNT] = {", out);
    for (size_t i = 0; i < compiled->plan.nodes_len; i++) {
        if (i > 0u)
            fputs(", ", out);
        write_c_string(out, compiled->plan.nodes[i].id);
    }
    fputs("};\n", out);
    return fclose(out) == 0;
}

static int export_wasm_skeleton(const char *project_path, const char *out_dir) {
    fputs("{\"schema\":\"apg.project.export.v1\",\"ok\":false,\"file\":", stdout);
    write_json_string(stdout, project_path);
    fputs(",\"target\":\"wasm_realtime\",\"out_dir\":", stdout);
    write_json_string(stdout, out_dir);
    fputs(
        ",\"diagnostics\":[{\"code\":\"APG_EXPORT_BLOCKED\",\"message\":\"wasm_realtime export command is reserved; "
        "WASM AudioWorklet bundle generation is not implemented yet\"}]}\n",
        stdout
    );
    return 1;
}

static int export_m7_static(const char *project_path, const char *out_dir) {
    uc_arena arena;
    if (uc_arena_init(&arena, 2 * 1024 * 1024) != 0) {
        uc_error err = {.status = UC_E_OOM};
        return write_cli_error(stdout, "apg.project.export.v1", project_path, "m7_static", &err);
    }

    apg_project_v2_resolved_t project;
    apg_project_v2_compiled_t compiled;
    uc_error                  err    = {0};
    uc_status                 status = load_compile_project(project_path, &arena, &project, &compiled, &err);
    if (status != UC_OK) {
        int rc = write_cli_error(stdout, "apg.project.export.v1", project_path, "m7_static", &err);
        uc_arena_free(&arena);
        return rc;
    }

    const apg_project_v2_loaded_unit_t *unsupported = first_unsupported_unit(&project, "m7_static");
    if (unsupported) {
        uc_error_set(&err, UC_E_TYPE, (uc_loc){0, 0}, "unit '%s' does not support m7_static", unsupported->id);
        int rc = write_cli_error(stdout, "apg.project.export.v1", project_path, "m7_static", &err);
        uc_arena_free(&arena);
        return rc;
    }

    char header_path[512];
    char source_path[512];
    if (!join_path(header_path, sizeof(header_path), out_dir, "apg_project_m7.h") ||
        !join_path(source_path, sizeof(source_path), out_dir, "apg_project_m7.c")) {
        uc_error_set(&err, UC_E_RANGE, (uc_loc){0, 0}, "export output path is too long");
        int rc = write_cli_error(stdout, "apg.project.export.v1", project_path, "m7_static", &err);
        uc_arena_free(&arena);
        return rc;
    }

    if (!write_m7_header(header_path, &compiled) || !write_m7_source(source_path, &project, &compiled)) {
        uc_error_set(&err, UC_E_IO, (uc_loc){0, 0}, "failed to write m7_static export files");
        int rc = write_cli_error(stdout, "apg.project.export.v1", project_path, "m7_static", &err);
        uc_arena_free(&arena);
        return rc;
    }

    fputs("{\"schema\":\"apg.project.export.v1\",\"ok\":true,\"file\":", stdout);
    write_json_string(stdout, project_path);
    fputs(",\"target\":\"m7_static\",\"out_dir\":", stdout);
    write_json_string(stdout, out_dir);
    fprintf(
        stdout, ",\"files\":[\"apg_project_m7.h\",\"apg_project_m7.c\"],\"nodes\":%zu,\"schedule\":%zu}\n",
        compiled.plan.nodes_len, compiled.plan.schedule_len
    );
    uc_arena_free(&arena);
    return 0;
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

    if (strcmp(argv[1], "benchmark") == 0) {
        if (argc != 4 || strcmp(argv[2], "project") != 0)
            return usage(argv[0]);
        return benchmark_project(argv[3]);
    }

    if (strcmp(argv[1], "export") == 0) {
        if (argc != 6 || strcmp(argv[2], "--target") != 0)
            return usage(argv[0]);
        if (strcmp(argv[3], "wasm_realtime") == 0)
            return export_wasm_skeleton(argv[4], argv[5]);
        if (strcmp(argv[3], "m7_static") == 0)
            return export_m7_static(argv[4], argv[5]);
        uc_error err = {0};
        uc_error_set(&err, UC_E_TYPE, (uc_loc){0, 0}, "unsupported export target");
        return write_cli_error(stdout, "apg.project.export.v1", argv[4], argv[3], &err);
    }

    return usage(argv[0]);
}
