#include <apgcore/json_contract_v2.h>

#include <apgcore/atom_catalog.h>
#include <apgcore/compiler_v2.h>
#include <apgcore/project_compiler_v2.h>
#include <apgcore/project_v2.h>
#include <apgcore/runtime_v2.h>
#include <apgcore/unit_v2.h>

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

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

static void write_validation_ok(FILE *out, const char *schema, const char *file) {
    fputs("{\"schema\":", out);
    write_json_string(out, schema);
    fputs(",\"ok\":true,\"file\":", out);
    write_json_string(out, file);
    fputs(",\"errors\":[],\"warnings\":[]}", out);
}

static void
write_validation_error(FILE *out, const char *schema, const char *file, const char *path, const uc_error *err) {
    fputs("{\"schema\":", out);
    write_json_string(out, schema);
    fputs(",\"ok\":false,\"file\":", out);
    write_json_string(out, file);
    fputs(",\"errors\":[{\"code\":", out);
    write_json_string(out, status_code(err ? err->status : UC_E_TYPE));
    fputs(",\"file\":", out);
    write_json_string(out, file);
    fputs(",\"path\":", out);
    write_json_string(out, path ? path : "$.");
    fputs(",\"message\":", out);
    write_json_string(out, err && err->msg[0] ? err->msg : "validation failed");
    fputs("}],\"warnings\":[]}", out);
}

void apg_v2_json_write_validate_unit(FILE *out, const char *path) {
    if (!out)
        return;
    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0) {
        uc_error err = {.status = UC_E_OOM};
        write_validation_error(out, "apg.validation.v1", path, "$.unit", &err);
        return;
    }

    apg_unit_v2_t unit;
    uc_error      err    = {0};
    uc_status     status = apg_unit_v2_load_file(path, &arena, &unit, &err);
    if (status == UC_OK) {
        apg_v2_compiled_unit_t plan;
        status = apg_v2_compile_unit(&unit, &arena, &plan, &err);
    }
    if (status == UC_OK)
        write_validation_ok(out, "apg.validation.v1", path);
    else
        write_validation_error(out, "apg.validation.v1", path, "$.unit", &err);
    uc_arena_free(&arena);
}

void apg_v2_json_write_validate_project(FILE *out, const char *path) {
    if (!out)
        return;
    uc_arena arena;
    if (uc_arena_init(&arena, 2 * 1024 * 1024) != 0) {
        uc_error err = {.status = UC_E_OOM};
        write_validation_error(out, "apg.validation.v1", path, "$.project", &err);
        return;
    }

    apg_project_v2_resolved_t project;
    uc_error                  err    = {0};
    uc_status                 status = apg_project_v2_load_resolved_file(path, &arena, &project, &err);
    if (status == UC_OK) {
        apg_project_v2_compiled_t compiled;
        status = apg_project_v2_compile(&project, &arena, &compiled, &err);
    }
    if (status == UC_OK)
        write_validation_ok(out, "apg.validation.v1", path);
    else
        write_validation_error(out, "apg.validation.v1", path, "$.project", &err);
    uc_arena_free(&arena);
}

static void write_optional_scalar_field(FILE *out, const char *key, const char *value) {
    if (!value)
        return;
    fputs(",\"", out);
    fputs(key, out);
    fputs("\":", out);
    write_json_string(out, value);
}

static void write_string_array(FILE *out, const char *const *items, size_t items_len) {
    fputc('[', out);
    for (size_t i = 0; i < items_len; i++) {
        if (i > 0u)
            fputc(',', out);
        write_json_string(out, items[i]);
    }
    fputc(']', out);
}

static void write_compatibility(FILE *out, const apg_unit_v2_t *unit) {
    fputc('{', out);
    for (size_t i = 0; unit && i < unit->compatibility_len; i++) {
        if (i > 0u)
            fputc(',', out);
        write_json_string(out, unit->compatibility[i].target);
        fputc(':', out);
        fputs(strcmp(unit->compatibility[i].supported, "true") == 0 ? "true" : "false", out);
    }
    fputc('}', out);
}

static void write_unit_params(FILE *out, const apg_unit_v2_t *unit) {
    fputc('[', out);
    for (size_t i = 0; i < unit->params_len; i++) {
        const apg_unit_v2_param_t *param = &unit->params[i];
        if (i > 0u)
            fputc(',', out);
        fputs("{\"name\":", out);
        write_json_string(out, param->name);
        fputs(",\"type\":", out);
        write_json_string(out, param->type);
        fputs(",\"default\":", out);
        write_json_string(out, param->default_value);
        write_optional_scalar_field(out, "min", param->min_value);
        write_optional_scalar_field(out, "max", param->max_value);
        write_optional_scalar_field(out, "smoothing_ms", param->smoothing_ms);
        if (param->ui_label || param->ui_control || param->ui_unit || param->ui_scale || param->ui_display_precision) {
            fputs(",\"ui\":{", out);
            bool first = true;
#define WRITE_UI_FIELD(name, value)        \
    do {                                   \
        if (value) {                       \
            if (!first)                    \
                fputc(',', out);           \
            first = false;                 \
            fputs("\"" name "\":", out);   \
            write_json_string(out, value); \
        }                                  \
    } while (0)
            WRITE_UI_FIELD("label", param->ui_label);
            WRITE_UI_FIELD("control", param->ui_control);
            WRITE_UI_FIELD("unit", param->ui_unit);
            WRITE_UI_FIELD("scale", param->ui_scale);
            WRITE_UI_FIELD("display_precision", param->ui_display_precision);
#undef WRITE_UI_FIELD
            fputc('}', out);
        }
        fputc('}', out);
    }
    fputc(']', out);
}

static void write_ports(FILE *out, const apg_unit_v2_port_t *ports, size_t ports_len) {
    fputc('[', out);
    for (size_t i = 0; i < ports_len; i++) {
        if (i > 0u)
            fputc(',', out);
        fputs("{\"name\":", out);
        write_json_string(out, ports[i].name);
        fputs(",\"type\":", out);
        write_json_string(out, ports[i].type);
        write_optional_scalar_field(out, "channels", ports[i].channels);
        if (ports[i].signals_len > 0u) {
            fputs(",\"signals\":", out);
            write_string_array(out, ports[i].signals, ports[i].signals_len);
        }
        write_optional_scalar_field(out, "target_kind", ports[i].target_kind);
        write_optional_scalar_field(out, "target_name", ports[i].target_name);
        fputc('}', out);
    }
    fputc(']', out);
}

static void write_unit_nodes(FILE *out, const apg_unit_v2_t *unit) {
    fputc('[', out);
    for (size_t i = 0; i < unit->nodes_len; i++) {
        if (i > 0u)
            fputc(',', out);
        fputs("{\"id\":", out);
        write_json_string(out, unit->nodes[i].id);
        fputs(",\"atom\":", out);
        write_json_string(out, unit->nodes[i].atom);
        fprintf(
            out, ",\"bindings\":{\"in\":%zu,\"out\":%zu,\"config\":%zu}}", unit->nodes[i].in_len,
            unit->nodes[i].out_len, unit->nodes[i].config_len
        );
    }
    fputc(']', out);
}

static void write_unit_inspect(FILE *out, const char *path, const apg_unit_v2_t *unit) {
    fputs("{\"schema\":\"apg.unit.inspect.v1\",\"file\":", out);
    write_json_string(out, path);
    fputs(",\"name\":", out);
    write_json_string(out, unit->name);
    fputs(",\"version\":", out);
    write_json_string(out, unit->version);
    fputs(",\"meta\":{\"title\":", out);
    write_json_string(out, unit->meta.title);
    fputs(",\"category\":", out);
    write_json_string(out, unit->meta.category);
    fputs(",\"description\":", out);
    write_json_string(out, unit->meta.description);
    fputs("},\"compatibility\":", out);
    write_compatibility(out, unit);
    fputs(",\"params\":", out);
    write_unit_params(out, unit);
    fputs(",\"ports\":{\"inputs\":", out);
    write_ports(out, unit->input_ports, unit->input_ports_len);
    fputs(",\"outputs\":", out);
    write_ports(out, unit->output_ports, unit->output_ports_len);
    fputs("},\"graph\":{\"signals\":", out);
    write_string_array(out, unit->signals, unit->signals_len);
    fputs(",\"nodes\":", out);
    write_unit_nodes(out, unit);
    fputs("}}", out);
}

void apg_v2_json_write_inspect_unit(FILE *out, const char *path) {
    if (!out)
        return;
    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0) {
        fputs("{\"schema\":\"apg.unit.inspect.v1\",\"ok\":false}", out);
        return;
    }
    apg_unit_v2_t unit;
    uc_error      err = {0};
    if (apg_unit_v2_load_file(path, &arena, &unit, &err) == UC_OK)
        write_unit_inspect(out, path, &unit);
    else
        write_validation_error(out, "apg.unit.inspect.v1", path, "$.unit", &err);
    uc_arena_free(&arena);
}

static void write_project_units(FILE *out, const apg_project_v2_resolved_t *project) {
    fputc('[', out);
    for (size_t i = 0; i < project->units_len; i++) {
        if (i > 0u)
            fputc(',', out);
        fputs("{\"id\":", out);
        write_json_string(out, project->units[i].id);
        fputs(",\"file\":", out);
        write_json_string(out, project->units[i].file);
        fputs(",\"name\":", out);
        write_json_string(out, project->units[i].unit.name);
        fputs(",\"compatibility\":", out);
        write_compatibility(out, &project->units[i].unit);
        fputc('}', out);
    }
    fputc(']', out);
}

static void write_project_nodes(FILE *out, const apg_project_v2_t *project) {
    fputc('[', out);
    for (size_t i = 0; i < project->nodes_len; i++) {
        if (i > 0u)
            fputc(',', out);
        fputs("{\"id\":", out);
        write_json_string(out, project->nodes[i].id);
        fputs(",\"unit\":", out);
        write_json_string(out, project->nodes[i].unit);
        fputs(",\"params\":[", out);
        for (size_t p = 0; p < project->nodes[i].params_len; p++) {
            if (p > 0u)
                fputc(',', out);
            fputs("{\"key\":", out);
            write_json_string(out, project->nodes[i].params[p].key);
            fputs(",\"value\":", out);
            write_json_string(out, project->nodes[i].params[p].value.text);
            fputc('}', out);
        }
        fputs("]}", out);
    }
    fputc(']', out);
}

static void write_routes(FILE *out, const apg_project_v2_t *project) {
    fputc('[', out);
    for (size_t i = 0; i < project->routes_len; i++) {
        if (i > 0u)
            fputc(',', out);
        fputs("{\"from\":", out);
        write_json_string(out, project->routes[i].from);
        fputs(",\"to\":", out);
        write_json_string(out, project->routes[i].to);
        fputc('}', out);
    }
    fputc(']', out);
}

static void write_project_inspect(
    FILE *out, const char *path, const apg_project_v2_resolved_t *project, const apg_project_v2_compiled_t *compiled
) {
    fputs("{\"schema\":\"apg.project.inspect.v1\",\"file\":", out);
    write_json_string(out, path);
    fputs(",\"name\":", out);
    write_json_string(out, project->project.name);
    fputs(",\"version\":", out);
    write_json_string(out, project->project.version);
    fputs(",\"units\":", out);
    write_project_units(out, project);
    fputs(",\"nodes\":", out);
    write_project_nodes(out, &project->project);
    fputs(",\"routes\":", out);
    write_routes(out, &project->project);
    fputs(",\"targets\":{\"default\":", out);
    write_json_string(out, project->project.targets.default_profile);
    fputs(",\"export\":", out);
    write_string_array(out, project->project.targets.export_profiles, project->project.targets.export_profiles_len);
    fprintf(
        out, "},\"compiled\":{\"params\":%zu,\"signals\":%zu,\"nodes\":%zu,\"schedule\":%zu}}",
        compiled->expanded_unit.params_len, compiled->expanded_unit.signals_len, compiled->plan.nodes_len,
        compiled->plan.schedule_len
    );
}

void apg_v2_json_write_inspect_project(FILE *out, const char *path) {
    if (!out)
        return;
    uc_arena arena;
    if (uc_arena_init(&arena, 2 * 1024 * 1024) != 0) {
        fputs("{\"schema\":\"apg.project.inspect.v1\",\"ok\":false}", out);
        return;
    }
    apg_project_v2_resolved_t project;
    apg_project_v2_compiled_t compiled;
    uc_error                  err    = {0};
    uc_status                 status = apg_project_v2_load_resolved_file(path, &arena, &project, &err);
    if (status == UC_OK)
        status = apg_project_v2_compile(&project, &arena, &compiled, &err);
    if (status == UC_OK)
        write_project_inspect(out, path, &project, &compiled);
    else
        write_validation_error(out, "apg.project.inspect.v1", path, "$.project", &err);
    uc_arena_free(&arena);
}

#define APG_RENDER_FRAMES      16u
#define APG_RENDER_SAMPLE_RATE 48000.0f

static void fill_deterministic_render_input(float *input, uint32_t frames) {
    static const float pattern[APG_RENDER_FRAMES] = {
        0.0f, 0.25f, 0.5f, -0.25f, 0.75f, -0.5f, 0.125f, 0.375f, 0.0f, -0.75f, 0.6f, -0.1f, 0.3f, -0.4f, 0.2f, 0.0f,
    };
    for (uint32_t i = 0; i < frames; i++)
        input[i] = pattern[i % APG_RENDER_FRAMES];
}

static void write_project_render(FILE *out, const char *path, const float *output, uint32_t frames) {
    float  peak       = 0.0f;
    double sum        = 0.0;
    double sum_square = 0.0;
    for (uint32_t i = 0; i < frames; i++) {
        float sample     = output[i];
        float abs_sample = fabsf(sample);
        if (abs_sample > peak)
            peak = abs_sample;
        sum += sample;
        sum_square += (double)sample * (double)sample;
    }
    double rms = frames > 0u ? sqrt(sum_square / (double)frames) : 0.0;

    fputs("{\"schema\":\"apg.project.render.v1\",\"ok\":true,\"file\":", out);
    write_json_string(out, path);
    fputs(",\"input\":\"deterministic_mono_v1\",\"sample_rate\":", out);
    fprintf(out, "%.0f", (double)APG_RENDER_SAMPLE_RATE);
    fputs(",\"frames\":", out);
    fprintf(out, "%u", (unsigned)frames);
    fputs(",\"output\":{\"peak\":", out);
    fprintf(out, "%.6f", (double)peak);
    fputs(",\"rms\":", out);
    fprintf(out, "%.6f", rms);
    fputs(",\"sum\":", out);
    fprintf(out, "%.6f", sum);
    fputs(",\"samples\":[", out);
    for (uint32_t i = 0; i < frames; i++) {
        if (i > 0u)
            fputc(',', out);
        fprintf(out, "%.6f", (double)output[i]);
    }
    fputs("]}}", out);
}

void apg_v2_json_write_render_project(FILE *out, const char *path) {
    if (!out)
        return;
    uc_arena arena;
    if (uc_arena_init(&arena, 2 * 1024 * 1024) != 0) {
        uc_error err = {.status = UC_E_OOM};
        write_validation_error(out, "apg.project.render.v1", path, "$.project", &err);
        return;
    }

    apg_project_v2_resolved_t project;
    apg_project_v2_compiled_t compiled;
    apg_v2_runtime_t          runtime       = {0};
    bool                      runtime_ready = false;
    uc_error                  err           = {0};
    uc_status                 status        = apg_project_v2_load_resolved_file(path, &arena, &project, &err);
    if (status == UC_OK)
        status = apg_project_v2_compile(&project, &arena, &compiled, &err);
    if (status == UC_OK) {
        status        = apg_v2_runtime_init(&compiled.plan, APG_RENDER_FRAMES, APG_RENDER_SAMPLE_RATE, &runtime, &err);
        runtime_ready = status == UC_OK;
    }

    float input[APG_RENDER_FRAMES];
    float output[APG_RENDER_FRAMES] = {0};
    if (status == UC_OK) {
        fill_deterministic_render_input(input, APG_RENDER_FRAMES);
        if (!apg_v2_runtime_process_mono_ports(&runtime, "input", input, "output", output, APG_RENDER_FRAMES)) {
            const char *msg = apg_v2_runtime_last_error(&runtime);
            uc_error_set(&err, UC_E_TYPE, (uc_loc){0, 0}, "%s", msg ? msg : "project render failed");
            status = UC_E_TYPE;
        }
    }

    if (status == UC_OK)
        write_project_render(out, path, output, APG_RENDER_FRAMES);
    else
        write_validation_error(out, "apg.project.render.v1", path, "$.project", &err);

    if (runtime_ready)
        apg_v2_runtime_destroy(&runtime);
    uc_arena_free(&arena);
}
