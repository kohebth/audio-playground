#include <apgcore/atom_catalog.h>
#include <apgcore/json_contract_v2.h>
#include <apgcore/project_compiler_v2.h>
#include <apgcore/project_v2.h>
#include <apgcore/registry/registry_builder_v2.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
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
        "  %s export --target wasm_realtime [--block-frames <frames>] [--sample-rate <hz>] <project> <outdir>\n"
        "  %s export --target m7_static <project> <outdir>\n"
        "  %s export --target m7_static [--max-static-ram <bytes>] [--block-frames <frames>] [--sample-rate <hz>] "
        "[--cache-line-bytes <bytes>] <project> <outdir>\n",
        argv0, argv0, argv0, argv0, argv0, argv0, argv0, argv0, argv0, argv0
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
        return write_cli_error(stdout, "apg.project.benchmark.v2", path, NULL, &err);
    }

    apg_project_v2_resolved_t project;
    apg_project_v2_compiled_t compiled;
    uc_error                  err    = {0};
    uc_status                 status = load_compile_project(path, &arena, &project, &compiled, &err);
    if (status != UC_OK) {
        int rc = write_cli_error(stdout, "apg.project.benchmark.v2", path, NULL, &err);
        uc_arena_free(&arena);
        return rc;
    }

    fputs("{\"schema\":\"apg.project.benchmark.v2\",\"ok\":true,\"file\":", stdout);
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

typedef enum {
    APG_TARGET_SUPPORT_SUPPORTED,
    APG_TARGET_SUPPORT_UNSUPPORTED,
    APG_TARGET_SUPPORT_UNDECLARED,
} apg_target_support_t;

static apg_target_support_t unit_profile_support(const apg_unit_v2_t *unit, const char *target) {
    for (size_t i = 0; unit && i < unit->compatibility_len; i++) {
        if (!unit->compatibility[i].target || strcmp(unit->compatibility[i].target, target) != 0)
            continue;
        if (unit->compatibility[i].supported && strcmp(unit->compatibility[i].supported, "true") == 0)
            return APG_TARGET_SUPPORT_SUPPORTED;
        return APG_TARGET_SUPPORT_UNSUPPORTED;
    }
    return APG_TARGET_SUPPORT_UNDECLARED;
}

static const apg_project_v2_loaded_unit_t *
first_unsupported_unit(const apg_project_v2_resolved_t *project, const char *target, const char **reason) {
    for (size_t i = 0; i < project->units_len; i++) {
        apg_target_support_t support = unit_profile_support(&project->units[i].unit, target);
        if (support == APG_TARGET_SUPPORT_SUPPORTED)
            continue;
        if (reason) {
            if (support == APG_TARGET_SUPPORT_UNSUPPORTED) {
                if (strcmp(target, "wasm_realtime") == 0)
                    *reason = "does not support wasm_realtime";
                else if (strcmp(target, "m7_static") == 0)
                    *reason = "does not support m7_static";
                else
                    *reason = "does not support target profile";
            } else {
                if (strcmp(target, "m7_static") == 0)
                    *reason = "does not declare m7_static compatibility";
                else if (strcmp(target, "wasm_realtime") == 0)
                    *reason = "does not declare wasm_realtime compatibility";
                else
                    *reason = "does not declare target profile compatibility";
            }
        }
        return &project->units[i];
    }
    return NULL;
}

static const char *first_unsupported_atom(const apg_v2_registry_t *registry, const char *target) {
    for (size_t i = 0; registry && i < registry->nodes_len; i++) {
        const char *atom_name = registry->node_layouts[i].atom_name;
        if (!apg_atom_profile_supported(atom_name, target))
            return atom_name;
    }
    return NULL;
}

static bool join_path(char *out, size_t out_size, const char *dir, const char *name) {
    size_t dir_len = dir ? strlen(dir) : 0u;
    int    written =
        snprintf(out, out_size, "%s%s%s", dir ? dir : "", dir_len > 0u && dir[dir_len - 1u] == '/' ? "" : "/", name);
    return written >= 0 && (size_t)written < out_size;
}

enum {
    APG_M7_DEFAULT_BLOCK_FRAMES = 64u,
    APG_M7_DEFAULT_SAMPLE_RATE  = 48000u,
    APG_M7_DEFAULT_CACHE_LINE   = 32u,
};

enum {
    APG_WASM_DEFAULT_BLOCK_FRAMES = 64u,
    APG_WASM_DEFAULT_SAMPLE_RATE  = 48000u,
};

typedef struct {
    uint32_t block_frames;
    uint32_t sample_rate;
    uint32_t cache_line_bytes;
    size_t   static_ram_budget;
    bool     has_static_ram_budget;
} m7_export_options_t;

typedef struct {
    size_t signal_buffer_bytes;
    size_t param_bytes;
    size_t schedule_bytes;
    size_t atom_call_bytes;
    size_t signal_array_pointer_count;
    size_t signal_array_pointer_bytes;
    size_t atom_storage_bytes;
    size_t state_buffer_bytes;
    size_t static_ram_bytes;
} m7_memory_manifest_t;

typedef struct {
    uint32_t block_frames;
    uint32_t sample_rate;
} wasm_export_options_t;

static bool parse_size_arg(const char *text, size_t *out) {
    if (!text || !out || text[0] == '\0')
        return false;
    char              *end   = NULL;
    unsigned long long value = strtoull(text, &end, 10);
    if (!end || *end != '\0' || value > (unsigned long long)SIZE_MAX)
        return false;
    *out = (size_t)value;
    return true;
}

static bool parse_uint32_arg(const char *text, uint32_t *out) {
    size_t value = 0u;
    if (!parse_size_arg(text, &value) || value == 0u || value > UINT32_MAX)
        return false;
    *out = (uint32_t)value;
    return true;
}

static bool write_wasm_runtime_js(
    const char                      *path,
    const apg_project_v2_resolved_t *project,
    const apg_v2_registry_t         *registry,
    const wasm_export_options_t     *options
) {
    FILE *out = fopen(path, "w");
    if (!out)
        return false;
    fputs("export const profile = \"wasm_realtime\";\n", out);
    fputs("export const command = \"audio-worklet-stub\";\n", out);
    if (project && project->project.name)
        fprintf(out, "export const projectName = \"%s\";\n", project->project.name);
    else
        fputs("export const projectName = \"unknown\";\n", out);
    fputs("export const runtime = {\n", out);
    fprintf(out, "  blockFrames: %uu,\n", options ? options->block_frames : APG_WASM_DEFAULT_BLOCK_FRAMES);
    fprintf(out, "  sampleRate: %uu,\n", options ? options->sample_rate : APG_WASM_DEFAULT_SAMPLE_RATE);
    if (project && project->project.name)
        fprintf(out, "  project: \"%s\",\n", project->project.name);
    else
        fputs("  project: \"unknown\",\n", out);
    if (project)
        fprintf(out, "  units: %zu,\n", project->units_len);
    if (registry)
        fprintf(out, "  nodes: %zu,\n  schedule: %zu,\n", registry->nodes_len, registry->schedule_len);
    fputs("};\n", out);
    fputs(
        "export function createRuntime() {\n"
        "  return {\n"
        "    compile() {\n"
        "      return Promise.resolve({ok: true, message: \"stub compile complete\"});\n"
        "    },\n"
        "    start() {\n"
        "      return Promise.reject(new Error(\"WASM AudioWorklet runtime execution not yet implemented\"));\n"
        "    },\n"
        "    stop() {\n"
        "      return Promise.resolve({ok: true});\n"
        "    },\n"
        "    setParam() {\n"
        "      return Promise.resolve({ok: true});\n"
        "    },\n"
        "    setBypass() {\n"
        "      return Promise.resolve({ok: true});\n"
        "    },\n"
        "    pollMeters() {\n"
        "      return Promise.resolve({peak: [], rms: []});\n"
        "    }\n"
        "  };\n"
        "}\n",
        out
    );
    return fclose(out) == 0;
}

static bool write_wasm_runtime_manifest(
    const char                      *path,
    const apg_project_v2_resolved_t *project,
    const apg_v2_registry_t         *registry,
    const wasm_export_options_t     *options
) {
    FILE *out = fopen(path, "w");
    if (!out)
        return false;
    fputs("{\"schema\":\"apg.project.wasm_realtime.v2\",\"project\":", out);
    write_json_string(out, project && project->project.name ? project->project.name : "unknown");
    fprintf(
        out,
        ",\"sample_rate\":%u,\"block_frames\":%u,\"runtime\":\"audio_worklet_stub\","
        "\"layout\":{\"params\":%zu,\"signals\":%zu,\"nodes\":%zu,\"schedule\":%zu},\"status\":\"generated\"}\n",
        options ? options->sample_rate : APG_WASM_DEFAULT_SAMPLE_RATE,
        options ? options->block_frames : APG_WASM_DEFAULT_BLOCK_FRAMES, registry ? registry->params_len : 0u,
        registry ? registry->signals_len : 0u, registry ? registry->nodes_len : 0u,
        registry ? registry->schedule_len : 0u
    );
    return fclose(out) == 0;
}

static bool parse_alignment_arg(const char *text, uint32_t *out) {
    uint32_t value = 0u;
    if (!parse_uint32_arg(text, &value) || (value & (value - 1u)) != 0u)
        return false;
    *out = value;
    return true;
}

static m7_memory_manifest_t m7_memory_manifest(const apg_v2_registry_t *registry) {
    m7_memory_manifest_t memory = {0};
    if (!registry)
        return memory;

    memory.signal_buffer_bytes        = registry->signal_samples * sizeof(float);
    memory.param_bytes                = registry->params_len * sizeof(float);
    memory.schedule_bytes             = registry->schedule_len * sizeof(uint32_t);
    memory.atom_call_bytes            = registry->nodes_len * sizeof(atom_call_t);
    memory.signal_array_pointer_count = registry->signal_array_pointer_slots;
    memory.signal_array_pointer_bytes = registry->signal_array_pointer_slots * sizeof(float *);
    memory.atom_storage_bytes         = registry->atom_storage_bytes;
    memory.state_buffer_bytes         = registry->state_buffer_samples * sizeof(float);
    memory.static_ram_bytes           = memory.signal_buffer_bytes + memory.param_bytes + memory.atom_call_bytes +
                              memory.signal_array_pointer_bytes + memory.atom_storage_bytes + memory.state_buffer_bytes;
    return memory;
}

static bool write_m7_header(
    const char                 *path,
    const apg_v2_registry_t    *registry,
    const m7_memory_manifest_t *memory,
    const m7_export_options_t  *options
) {
    FILE *out = fopen(path, "w");
    if (!out)
        return false;
    fputs("#ifndef APG_PROJECT_M7_BUNDLE_H\n#define APG_PROJECT_M7_BUNDLE_H\n\n", out);
    fputs("#include <stddef.h>\n#include <stdint.h>\n#include <atom_registry.h>\n\n", out);
    fprintf(out, "#define APG_M7_PROJECT_PARAM_COUNT %zuu\n", registry->params_len);
    fprintf(out, "#define APG_M7_PROJECT_SIGNAL_COUNT %zuu\n", registry->signals_len);
    fprintf(out, "#define APG_M7_PROJECT_NODE_COUNT %zuu\n", registry->nodes_len);
    fprintf(out, "#define APG_M7_PROJECT_SCHEDULE_COUNT %zuu\n", registry->schedule_len);
    fprintf(out, "#define APG_M7_PROJECT_SIGNAL_ARRAY_POINTER_COUNT %zuu\n\n", memory->signal_array_pointer_count);
    fprintf(out, "#define APG_M7_PROJECT_BLOCK_FRAMES %uu\n", registry->frame_capacity);
    fprintf(out, "#define APG_M7_PROJECT_SAMPLE_RATE %uu\n", (unsigned)registry->sample_rate);
    fprintf(out, "#define APG_M7_PROJECT_ATOM_CALLS_PER_BLOCK %zuu\n", registry->schedule_len);
    fprintf(
        out, "#define APG_M7_PROJECT_ATOM_CALLS_PER_SECOND %zuu\n",
        registry->frame_capacity ? registry->schedule_len * (size_t)registry->sample_rate / registry->frame_capacity
                                 : 0u
    );
    fprintf(out, "#define APG_M7_PROJECT_SIGNAL_BUFFER_BYTES %zuu\n", memory->signal_buffer_bytes);
    fprintf(out, "#define APG_M7_PROJECT_PARAM_BYTES %zuu\n", memory->param_bytes);
    fprintf(out, "#define APG_M7_PROJECT_SCHEDULE_BYTES %zuu\n", memory->schedule_bytes);
    fprintf(out, "#define APG_M7_PROJECT_ATOM_CALL_BYTES %zuu\n", memory->atom_call_bytes);
    fprintf(out, "#define APG_M7_PROJECT_SIGNAL_ARRAY_POINTER_BYTES %zuu\n", memory->signal_array_pointer_bytes);
    fprintf(out, "#define APG_M7_PROJECT_ATOM_STORAGE_BYTES %zuu\n", memory->atom_storage_bytes);
    fprintf(out, "#define APG_M7_PROJECT_STATE_BUFFER_BYTES %zuu\n", memory->state_buffer_bytes);
    fprintf(out, "#define APG_M7_PROJECT_STATIC_RAM_BYTES %zuu\n\n", memory->static_ram_bytes);
    fprintf(out, "#define APG_M7_CACHE_LINE_BYTES %uu\n\n", options->cache_line_bytes);
    fputs("#define APG_M7_SECTION_SIGNAL_BUFFERS \".apg_m7_signal_buffers\"\n", out);
    fputs("#define APG_M7_SECTION_PARAMS \".apg_m7_params\"\n", out);
    fputs("#define APG_M7_SECTION_ATOM_CALLS \".apg_m7_atom_calls\"\n", out);
    fputs("#define APG_M7_SECTION_SIGNAL_ARRAYS \".apg_m7_signal_arrays\"\n", out);
    fputs("#define APG_M7_SECTION_ATOM_STORAGE \".apg_m7_atom_storage\"\n", out);
    fputs("#define APG_M7_SECTION_STATE_BUFFERS \".apg_m7_state_buffers\"\n", out);
    fputs("#if defined(__GNUC__)\n", out);
    fputs("#define APG_M7_SECTION_ATTR(name) __attribute__((section(name), aligned(APG_M7_CACHE_LINE_BYTES)))\n", out);
    fputs("#else\n#define APG_M7_SECTION_ATTR(name)\n#endif\n\n", out);
    fputs("#define APG_M7_PROJECT_USES_RUNTIME_YAML 0u\n", out);
    fputs("#define APG_M7_PROJECT_USES_DYNAMIC_ALLOCATION 0u\n\n", out);
    fputs("extern const char apg_m7_project_name[];\n", out);
    fputs("extern const uint32_t apg_m7_project_schedule[APG_M7_PROJECT_SCHEDULE_COUNT];\n", out);
    fputs("extern const char *const apg_m7_project_nodes[APG_M7_PROJECT_NODE_COUNT];\n", out);
    fputs("extern const char *const apg_m7_project_atom_process_symbols[APG_M7_PROJECT_NODE_COUNT];\n", out);
    fputs("extern const atom_thunk_fn apg_m7_project_atom_thunks[APG_M7_PROJECT_NODE_COUNT];\n", out);
    fputs("extern const apg_process_info_t apg_m7_project_process_info;\n", out);
    fputs("extern atom_call_t apg_m7_project_atom_calls[APG_M7_PROJECT_NODE_COUNT];\n", out);
    fputs("void apg_m7_project_init(void);\n", out);
    fputs("void apg_m7_project_refresh_params(void);\n", out);
    fputs("void apg_m7_project_process_block(void);\n", out);
    fputs("\n#endif\n", out);
    return fclose(out) == 0;
}

static void write_c_float(FILE *out, float value) {
    char text[48];
    snprintf(text, sizeof(text), "%.9g", (double)value);
    fputs(text, out);
    if (!strchr(text, '.') && !strchr(text, 'e') && !strchr(text, 'E'))
        fputs(".0", out);
    fputc('f', out);
}

static void write_m7_scalar_value(FILE *out, const apg_v2_registry_scalar_refresh_t *item) {
    if (!item) {
        fputs("0.0f", out);
    } else if (item->kind == APG_BIND_PARAM) {
        fprintf(out, "apg_m7_param(%zuu)", item->param_index);
    } else if (item->kind == APG_BIND_LITERAL) {
        write_c_float(out, item->number);
    } else {
        fputs("0.0f", out);
    }
}

static bool
write_m7_source(const char *path, const apg_project_v2_resolved_t *project, const apg_v2_registry_t *registry) {
    FILE *out = fopen(path, "w");
    if (!out)
        return false;
    fputs("#include \"apg_project_m7.h\"\n\n", out);
    fputs("#include <string.h>\n\n", out);
    fputs("#include <atom/dsp_types.h>\n\n", out);
    fputs("const char apg_m7_project_name[] = ", out);
    write_c_string(out, project->project.name);
    fputs(";\n\nconst uint32_t apg_m7_project_schedule[APG_M7_PROJECT_SCHEDULE_COUNT] = {", out);
    for (size_t i = 0; i < registry->schedule_len; i++) {
        if (i > 0u)
            fputs(", ", out);
        fprintf(out, "%uu", (unsigned)registry->schedule[i]);
    }
    fputs("};\n\nconst char *const apg_m7_project_nodes[APG_M7_PROJECT_NODE_COUNT] = {", out);
    for (size_t i = 0; i < registry->nodes_len; i++) {
        if (i > 0u)
            fputs(", ", out);
        write_c_string(out, registry->node_layouts[i].node_id);
    }
    fputs("};\n\n", out);
    fputs("const apg_process_info_t apg_m7_project_process_info = {", out);
    fprintf(
        out, ".sample_rate = %.1ff, .frames = %uu, .output_frames = %uu, .channels = 1u", (double)registry->sample_rate,
        registry->frame_capacity, registry->frame_capacity
    );
    fputs("};\n\n", out);
    for (size_t i = 0; i < registry->nodes_len; i++) {
        bool seen = false;
        // ponytail: O(n^2) is fine for generated project node counts; sort/dedupe if exports grow large.
        for (size_t j = 0; j < i; j++) {
            if (strcmp(registry->node_layouts[j].atom_name, registry->node_layouts[i].atom_name) == 0) {
                seen = true;
                break;
            }
        }
        if (seen)
            continue;
        fputs("extern void ", out);
        fputs(registry->node_layouts[i].atom_name, out);
        fputs("_thunk(atom_call_t *call);\n", out);
    }
    fputs("\nconst atom_thunk_fn apg_m7_project_atom_thunks[APG_M7_PROJECT_NODE_COUNT] = {", out);
    for (size_t i = 0; i < registry->nodes_len; i++) {
        if (i > 0u)
            fputs(", ", out);
        fputs(registry->node_layouts[i].atom_name, out);
        fputs("_thunk", out);
    }
    fputs("};\n\n", out);
    fputs("const char *const apg_m7_project_atom_process_symbols[APG_M7_PROJECT_NODE_COUNT] = {", out);
    for (size_t i = 0; i < registry->nodes_len; i++) {
        if (i > 0u)
            fputs(", ", out);
        fputc('"', out);
        fputs(registry->node_layouts[i].atom_name, out);
        fputs("_process\"", out);
    }
    fputs("};\n\n", out);
    fputs("#if APG_M7_PROJECT_SIGNAL_BUFFER_BYTES > 0u\n", out);
    fputs(
        "uint8_t apg_m7_project_signal_buffers[APG_M7_PROJECT_SIGNAL_BUFFER_BYTES] "
        "APG_M7_SECTION_ATTR(APG_M7_SECTION_SIGNAL_BUFFERS);\n",
        out
    );
    fputs("#endif\n\n#if APG_M7_PROJECT_PARAM_BYTES > 0u\n", out);
    fputs(
        "uint8_t apg_m7_project_params[APG_M7_PROJECT_PARAM_BYTES] "
        "APG_M7_SECTION_ATTR(APG_M7_SECTION_PARAMS);\n",
        out
    );
    fputs("#endif\n\n#if APG_M7_PROJECT_SIGNAL_ARRAY_POINTER_BYTES > 0u\n", out);
    fputs(
        "float *apg_m7_project_signal_array_pool[APG_M7_PROJECT_SIGNAL_ARRAY_POINTER_COUNT] "
        "APG_M7_SECTION_ATTR(APG_M7_SECTION_SIGNAL_ARRAYS);\n",
        out
    );
    fputs("#endif\n\n#if APG_M7_PROJECT_STATE_BUFFER_BYTES > 0u\n", out);
    fputs(
        "uint8_t apg_m7_project_state_buffers[APG_M7_PROJECT_STATE_BUFFER_BYTES] "
        "APG_M7_SECTION_ATTR(APG_M7_SECTION_STATE_BUFFERS);\n",
        out
    );
    fputs("#endif\n\n", out);
    for (size_t i = 0; i < registry->nodes_len; i++) {
        const apg_v2_registry_node_layout_t *layout = &registry->node_layouts[i];
        if (layout->mix_matrix_coefficients_len == 0u)
            continue;
        fprintf(out, "static float apg_m7_node%zu_mix_coefficients[%zu] = {", i, layout->mix_matrix_coefficients_len);
        for (size_t j = 0; j < layout->mix_matrix_coefficients_len; j++) {
            if (j > 0u)
                fputs(", ", out);
            write_c_float(out, layout->mix_matrix_coefficients[j]);
        }
        fputs("};\n", out);
        fprintf(out, "static float *apg_m7_node%zu_mix_rows[%zu] = {", i, layout->mix_matrix_num_out);
        for (size_t row = 0; row < layout->mix_matrix_num_out; row++) {
            if (row > 0u)
                fputs(", ", out);
            fprintf(out, "&apg_m7_node%zu_mix_coefficients[%zuu]", i, row * layout->mix_matrix_num_in);
        }
        fputs("};\n\n", out);
    }
    fputs("typedef struct {\n", out);
    for (size_t i = 0; i < registry->nodes_len; i++) {
        const char *atom_name = registry->node_layouts[i].atom_name;
        fprintf(out, "    %s_out_t node%zu_out;\n", atom_name, i);
        fprintf(out, "    %s_in_t node%zu_in;\n", atom_name, i);
        fprintf(out, "    %s_params_t node%zu_config;\n", atom_name, i);
        fprintf(out, "    %s_state_t node%zu_state;\n", atom_name, i);
    }
    fputs(
        "} apg_m7_project_atom_storage_t;\n\n"
        "apg_m7_project_atom_storage_t apg_m7_project_atom_storage "
        "APG_M7_SECTION_ATTR(APG_M7_SECTION_ATOM_STORAGE);\n\n",
        out
    );
    fputs(
        "atom_call_t apg_m7_project_atom_calls[APG_M7_PROJECT_NODE_COUNT] "
        "APG_M7_SECTION_ATTR(APG_M7_SECTION_ATOM_CALLS) = {",
        out
    );
    for (size_t i = 0; i < registry->nodes_len; i++) {
        if (i > 0u)
            fputs(", ", out);
        fprintf(
            out,
            "{.out = &apg_m7_project_atom_storage.node%zu_out, .in = &apg_m7_project_atom_storage.node%zu_in, "
            ".config = &apg_m7_project_atom_storage.node%zu_config, "
            ".state = &apg_m7_project_atom_storage.node%zu_state, .info = &apg_m7_project_process_info}",
            i, i, i, i
        );
    }
    fputs("};\n\n", out);
    fputs("#if APG_M7_PROJECT_SIGNAL_BUFFER_BYTES > 0u\n", out);
    fputs("static float *apg_m7_signal(size_t index) {\n", out);
    fputs(
        "    return (float *)(void *)&apg_m7_project_signal_buffers[index * APG_M7_PROJECT_BLOCK_FRAMES * "
        "sizeof(float)];\n",
        out
    );
    fputs("}\n#endif\n\n#if APG_M7_PROJECT_PARAM_BYTES > 0u\n", out);
    fputs("static float apg_m7_param(size_t index) {\n", out);
    fputs("    return ((float *)(void *)apg_m7_project_params)[index];\n}\n#endif\n\n", out);
    fputs("void apg_m7_project_refresh_params(void) {\n", out);
    for (size_t i = 0; i < registry->nodes_len; i++) {
        const apg_v2_registry_node_layout_t *layout = &registry->node_layouts[i];
        for (size_t j = 0; j < layout->config_refreshes_len; j++) {
            const apg_v2_registry_scalar_refresh_t *item = &layout->config_refreshes[j];
            const char                             *key  = item->key;
            if (!key)
                continue;
            fprintf(out, "    apg_m7_project_atom_storage.node%zu_config.%s = ", i, key);
            write_m7_scalar_value(out, item);
            fputs(";\n", out);
        }
        for (size_t j = 0; j < layout->input_refreshes_len; j++) {
            const apg_v2_registry_scalar_refresh_t *item = &layout->input_refreshes[j];
            const char                             *key  = item->key;
            if (!key)
                continue;
            fprintf(out, "    apg_m7_project_atom_storage.node%zu_in.%s = ", i, key);
            write_m7_scalar_value(out, item);
            fputs(";\n", out);
        }
    }
    fputs("}\n\nvoid apg_m7_project_init(void) {\n", out);
    fputs("    memset(&apg_m7_project_atom_storage, 0, sizeof(apg_m7_project_atom_storage));\n", out);
    fputs("#if APG_M7_PROJECT_SIGNAL_BUFFER_BYTES > 0u\n", out);
    fputs("    memset(apg_m7_project_signal_buffers, 0, APG_M7_PROJECT_SIGNAL_BUFFER_BYTES);\n", out);
    fputs("#endif\n#if APG_M7_PROJECT_PARAM_BYTES > 0u\n", out);
    fputs("    memset(apg_m7_project_params, 0, APG_M7_PROJECT_PARAM_BYTES);\n", out);
    fputs("#endif\n#if APG_M7_PROJECT_STATE_BUFFER_BYTES > 0u\n", out);
    fputs("    memset(apg_m7_project_state_buffers, 0, APG_M7_PROJECT_STATE_BUFFER_BYTES);\n", out);
    fputs("#endif\n", out);
    for (size_t i = 0; i < registry->params_len; i++) {
        fputs("    ((float *)(void *)apg_m7_project_params)[", out);
        fprintf(out, "%zuu] = ", i);
        write_c_float(out, registry->param_defaults ? registry->param_defaults[i] : 0.0f);
        fputs(";\n", out);
    }
    for (size_t i = 0; i < registry->nodes_len; i++) {
        const apg_v2_registry_node_layout_t *layout = &registry->node_layouts[i];
        for (size_t j = 0; j < layout->signal_bindings_len; j++) {
            const apg_v2_registry_signal_binding_t *binding = &layout->signal_bindings[j];
            const char                             *side    = binding->is_input ? "in" : "out";
            const char                             *key     = binding->key;
            if (!key)
                continue;
            if (binding->is_signal_array) {
                for (size_t k = 0; k < binding->signal_array_len; k++) {
                    fprintf(
                        out, "    apg_m7_project_signal_array_pool[%zuu] = apg_m7_signal(%zuu);\n",
                        layout->signal_array_pool_offset + binding->signal_array_offset + k,
                        binding->signal_array_indices[k]
                    );
                }
                fprintf(
                    out,
                    "    apg_m7_project_atom_storage.node%zu_%s.%s = "
                    "&apg_m7_project_signal_array_pool[%zuu];\n",
                    i, side, key, layout->signal_array_pool_offset + binding->signal_array_offset
                );
            } else {
                fprintf(
                    out, "    apg_m7_project_atom_storage.node%zu_%s.%s = apg_m7_signal(%zuu);\n", i, side, key,
                    binding->signal_index
                );
            }
        }
        size_t state_buffer_index = 0u;
        for (int field_index = 0; field_index < layout->n_state_fields; field_index++) {
            const atom_field_desc_t *field = &layout->state_fields[field_index];
            if (field->type != FIELD_BUFFER)
                continue;
            fprintf(
                out,
                "    apg_m7_project_atom_storage.node%zu_state.%s = "
                "(float *)(void *)&apg_m7_project_state_buffers[%zuu];\n",
                i, field->name, layout->state_buffer_sample_offsets_by_index[state_buffer_index] * sizeof(float)
            );
            state_buffer_index++;
        }
        if (layout->mix_matrix_coefficients_len > 0u) {
            fprintf(
                out, "    apg_m7_project_atom_storage.node%zu_config.coefficients = apg_m7_node%zu_mix_rows;\n", i, i
            );
            fprintf(
                out, "    apg_m7_project_atom_storage.node%zu_config.num_in = %zu;\n", i, layout->mix_matrix_num_in
            );
            fprintf(
                out, "    apg_m7_project_atom_storage.node%zu_config.num_out = %zu;\n", i, layout->mix_matrix_num_out
            );
        }
    }
    fputs("    apg_m7_project_refresh_params();\n}\n\n", out);
    fputs("void apg_m7_project_process_block(void) {\n", out);
    fputs("    for (size_t i = 0u; i < APG_M7_PROJECT_SCHEDULE_COUNT; i++) {\n", out);
    fputs("        uint32_t node = apg_m7_project_schedule[i];\n", out);
    fputs("        apg_m7_project_atom_thunks[node](&apg_m7_project_atom_calls[node]);\n", out);
    fputs("    }\n}\n", out);
    return fclose(out) == 0;
}

static int export_wasm_realtime(const char *project_path, const char *out_dir, const wasm_export_options_t *options) {
    uc_arena arena;
    if (uc_arena_init(&arena, 2 * 1024 * 1024) != 0) {
        uc_error err = {.status = UC_E_OOM};
        return write_cli_error(stdout, "apg.project.export.v2", project_path, "wasm_realtime", &err);
    }

    apg_project_v2_resolved_t project;
    apg_project_v2_compiled_t compiled;
    uc_error                  err    = {0};
    uc_status                 status = load_compile_project(project_path, &arena, &project, &compiled, &err);
    if (status != UC_OK) {
        int rc = write_cli_error(stdout, "apg.project.export.v2", project_path, "wasm_realtime", &err);
        uc_arena_free(&arena);
        return rc;
    }

    const char                         *unit_reason = NULL;
    const apg_project_v2_loaded_unit_t *unsupported = first_unsupported_unit(&project, "wasm_realtime", &unit_reason);
    if (unsupported) {
        uc_error_set(
            &err, UC_E_TYPE, (uc_loc){0, 0}, "unit '%s' %s", unsupported->id,
            unit_reason ? unit_reason : "does not support target profile"
        );
        int rc = write_cli_error(stdout, "apg.project.export.v2", project_path, "wasm_realtime", &err);
        uc_arena_free(&arena);
        return rc;
    }

    uc_arena          registry_arena = {0};
    apg_v2_registry_t registry       = {0};
    status                           = apg_v2_registry_build_with_growth(
        &compiled.plan, options->block_frames, (float)options->sample_rate, &registry_arena, &registry, &err
    );
    if (status != UC_OK) {
        int rc = write_cli_error(stdout, "apg.project.export.v2", project_path, "wasm_realtime", &err);
        uc_arena_free(&arena);
        return rc;
    }

    const char *unsupported_atom = first_unsupported_atom(&registry, "wasm_realtime");
    if (unsupported_atom) {
        uc_error_set(&err, UC_E_TYPE, (uc_loc){0, 0}, "atom '%s' does not support wasm_realtime", unsupported_atom);
        int rc = write_cli_error(stdout, "apg.project.export.v2", project_path, "wasm_realtime", &err);
        uc_arena_free(&registry_arena);
        uc_arena_free(&arena);
        return rc;
    }

    char manifest_path[512];
    char js_path[512];
    if (!join_path(manifest_path, sizeof(manifest_path), out_dir, "apg_project_wasm.json") ||
        !join_path(js_path, sizeof(js_path), out_dir, "apg_project_wasm.mjs")) {
        uc_error_set(&err, UC_E_RANGE, (uc_loc){0, 0}, "export output path is too long");
        int rc = write_cli_error(stdout, "apg.project.export.v2", project_path, "wasm_realtime", &err);
        uc_arena_free(&registry_arena);
        uc_arena_free(&arena);
        return rc;
    }

    if (!write_wasm_runtime_manifest(manifest_path, &project, &registry, options) ||
        !write_wasm_runtime_js(js_path, &project, &registry, options)) {
        uc_error_set(&err, UC_E_IO, (uc_loc){0, 0}, "failed to write wasm_realtime export files");
        int rc = write_cli_error(stdout, "apg.project.export.v2", project_path, "wasm_realtime", &err);
        uc_arena_free(&registry_arena);
        uc_arena_free(&arena);
        return rc;
    }

    fputs("{\"schema\":\"apg.project.export.v2\",\"ok\":true,\"file\":", stdout);
    write_json_string(stdout, project_path);
    fputs(",\"target\":\"wasm_realtime\",\"out_dir\":", stdout);
    write_json_string(stdout, out_dir);
    fputs(",\"status\":\"stub\",\"files\":[\"apg_project_wasm.json\",\"apg_project_wasm.mjs\"],\"nodes\":", stdout);
    fprintf(stdout, "%zu,\"schedule\":%zu", registry.nodes_len, registry.schedule_len);
    fprintf(
        stdout, ",\"execution\":{\"sample_rate\":%u,\"block_frames\":%u,\"atom_calls_per_block\":%zu,",
        options->sample_rate, options->block_frames, registry.schedule_len
    );
    fprintf(
        stdout, "\"atom_calls_per_second\":%zu}}\n",
        registry.schedule_len * options->sample_rate / options->block_frames
    );

    uc_arena_free(&registry_arena);
    uc_arena_free(&arena);
    return 0;
}

static int export_m7_static(const char *project_path, const char *out_dir, const m7_export_options_t *options) {
    uc_arena arena;
    if (uc_arena_init(&arena, 2 * 1024 * 1024) != 0) {
        uc_error err = {.status = UC_E_OOM};
        return write_cli_error(stdout, "apg.project.export.v2", project_path, "m7_static", &err);
    }

    apg_project_v2_resolved_t project;
    apg_project_v2_compiled_t compiled;
    uc_error                  err    = {0};
    uc_status                 status = load_compile_project(project_path, &arena, &project, &compiled, &err);
    if (status != UC_OK) {
        int rc = write_cli_error(stdout, "apg.project.export.v2", project_path, "m7_static", &err);
        uc_arena_free(&arena);
        return rc;
    }

    const char                         *unit_reason = NULL;
    const apg_project_v2_loaded_unit_t *unsupported = first_unsupported_unit(&project, "m7_static", &unit_reason);
    if (unsupported) {
        uc_error_set(
            &err, UC_E_TYPE, (uc_loc){0, 0}, "unit '%s' %s", unsupported->id,
            unit_reason ? unit_reason : "does not support m7_static"
        );
        int rc = write_cli_error(stdout, "apg.project.export.v2", project_path, "m7_static", &err);
        uc_arena_free(&arena);
        return rc;
    }

    uc_arena          registry_arena = {0};
    apg_v2_registry_t registry       = {0};
    status                           = apg_v2_registry_build_with_growth(
        &compiled.plan, options->block_frames, (float)options->sample_rate, &registry_arena, &registry, &err
    );
    if (status != UC_OK) {
        int rc = write_cli_error(stdout, "apg.project.export.v2", project_path, "m7_static", &err);
        uc_arena_free(&arena);
        return rc;
    }
    const char *unsupported_atom = first_unsupported_atom(&registry, "m7_static");
    if (unsupported_atom) {
        uc_error_set(&err, UC_E_TYPE, (uc_loc){0, 0}, "atom '%s' does not support m7_static", unsupported_atom);
        int rc = write_cli_error(stdout, "apg.project.export.v2", project_path, "m7_static", &err);
        uc_arena_free(&registry_arena);
        uc_arena_free(&arena);
        return rc;
    }
    m7_memory_manifest_t memory = m7_memory_manifest(&registry);
    if (options->has_static_ram_budget && memory.static_ram_bytes > options->static_ram_budget) {
        uc_error_set(
            &err, UC_E_RANGE, (uc_loc){0, 0}, "m7_static static RAM budget exceeded: %zu > %zu bytes",
            memory.static_ram_bytes, options->static_ram_budget
        );
        int rc = write_cli_error(stdout, "apg.project.export.v2", project_path, "m7_static", &err);
        uc_arena_free(&registry_arena);
        uc_arena_free(&arena);
        return rc;
    }

    char header_path[512];
    char source_path[512];
    if (!join_path(header_path, sizeof(header_path), out_dir, "apg_project_m7.h") ||
        !join_path(source_path, sizeof(source_path), out_dir, "apg_project_m7.c")) {
        uc_error_set(&err, UC_E_RANGE, (uc_loc){0, 0}, "export output path is too long");
        int rc = write_cli_error(stdout, "apg.project.export.v2", project_path, "m7_static", &err);
        uc_arena_free(&registry_arena);
        uc_arena_free(&arena);
        return rc;
    }

    if (!write_m7_header(header_path, &registry, &memory, options) ||
        !write_m7_source(source_path, &project, &registry)) {
        uc_error_set(&err, UC_E_IO, (uc_loc){0, 0}, "failed to write m7_static export files");
        int rc = write_cli_error(stdout, "apg.project.export.v2", project_path, "m7_static", &err);
        uc_arena_free(&registry_arena);
        uc_arena_free(&arena);
        return rc;
    }

    fputs("{\"schema\":\"apg.project.export.v2\",\"ok\":true,\"file\":", stdout);
    write_json_string(stdout, project_path);
    fputs(",\"target\":\"m7_static\",\"out_dir\":", stdout);
    write_json_string(stdout, out_dir);
    fprintf(
        stdout,
        ",\"files\":[\"apg_project_m7.h\",\"apg_project_m7.c\"],\"nodes\":%zu,\"schedule\":%zu,"
        "\"memory\":{\"block_frames\":%u,\"signal_buffer_bytes\":%zu,\"param_bytes\":%zu,"
        "\"schedule_bytes\":%zu,\"atom_call_bytes\":%zu,\"signal_array_pointer_bytes\":%zu,"
        "\"atom_storage_bytes\":%zu,\"state_buffer_bytes\":%zu,"
        "\"static_ram_bytes\":%zu,\"cache_line_bytes\":%u},\"execution\":{\"sample_rate\":%u,\"block_frames\":%u,"
        "\"atom_calls_per_block\":%zu,\"atom_calls_per_second\":%zu}}\n",
        registry.nodes_len, registry.schedule_len, options->block_frames, memory.signal_buffer_bytes,
        memory.param_bytes, memory.schedule_bytes, memory.atom_call_bytes, memory.signal_array_pointer_bytes,
        memory.atom_storage_bytes, memory.state_buffer_bytes, memory.static_ram_bytes, options->cache_line_bytes,
        options->sample_rate, options->block_frames, registry.schedule_len,
        registry.schedule_len * options->sample_rate / options->block_frames
    );
    uc_arena_free(&registry_arena);
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
        if (argc < 6 || strcmp(argv[2], "--target") != 0)
            return usage(argv[0]);
        if (strcmp(argv[3], "wasm_realtime") == 0) {
            wasm_export_options_t options = {
                .block_frames = APG_WASM_DEFAULT_BLOCK_FRAMES,
                .sample_rate  = APG_WASM_DEFAULT_SAMPLE_RATE,
            };
            int index = 4;
            while (index + 2 < argc && strncmp(argv[index], "--", 2) == 0) {
                if (strcmp(argv[index], "--block-frames") == 0) {
                    if (!parse_uint32_arg(argv[index + 1], &options.block_frames))
                        return usage(argv[0]);
                } else if (strcmp(argv[index], "--sample-rate") == 0) {
                    if (!parse_uint32_arg(argv[index + 1], &options.sample_rate))
                        return usage(argv[0]);
                } else {
                    return usage(argv[0]);
                }
                index += 2;
            }
            if (argc - index != 2)
                return usage(argv[0]);
            return export_wasm_realtime(argv[index], argv[index + 1], &options);
        }
        if (strcmp(argv[3], "m7_static") == 0) {
            m7_export_options_t options = {
                .block_frames     = APG_M7_DEFAULT_BLOCK_FRAMES,
                .sample_rate      = APG_M7_DEFAULT_SAMPLE_RATE,
                .cache_line_bytes = APG_M7_DEFAULT_CACHE_LINE,
            };
            int index = 4;
            while (index + 2 < argc && strncmp(argv[index], "--", 2) == 0) {
                if (strcmp(argv[index], "--max-static-ram") == 0) {
                    if (!parse_size_arg(argv[index + 1], &options.static_ram_budget))
                        return usage(argv[0]);
                    options.has_static_ram_budget = true;
                } else if (strcmp(argv[index], "--block-frames") == 0) {
                    if (!parse_uint32_arg(argv[index + 1], &options.block_frames))
                        return usage(argv[0]);
                } else if (strcmp(argv[index], "--sample-rate") == 0) {
                    if (!parse_uint32_arg(argv[index + 1], &options.sample_rate))
                        return usage(argv[0]);
                } else if (strcmp(argv[index], "--cache-line-bytes") == 0) {
                    if (!parse_alignment_arg(argv[index + 1], &options.cache_line_bytes))
                        return usage(argv[0]);
                } else {
                    return usage(argv[0]);
                }
                index += 2;
            }
            if (argc - index != 2)
                return usage(argv[0]);
            return export_m7_static(argv[index], argv[index + 1], &options);
        }
        uc_error err = {0};
        uc_error_set(&err, UC_E_TYPE, (uc_loc){0, 0}, "unsupported export target");
        return write_cli_error(stdout, "apg.project.export.v2", argv[4], argv[3], &err);
    }

    return usage(argv[0]);
}
