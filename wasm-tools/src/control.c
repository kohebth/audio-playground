#include <apg/wasm/abi.h>

#include <apgcore/compiler/project_compiler_v2.h>
#include <apgcore/registry/registry_builder_v2.h>
#include <apgcore/validator/project_v2.h>
#include <apgcore/validator/project_validator_v2.h>
#include <apgcore/validator/unit_v2.h>
#include <yaml/arena.h>
#include <yaml/error.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "image.h"

#define APG_WASM_DEFAULT_ARENA_BYTES (4u * 1024u * 1024u)
#define APG_WASM_DIAGNOSTIC_TEXT     128u

typedef struct {
    apg_wasm_file_role_t role;
    char                *path;
    char                *content;
    size_t               content_len;
} apg_wasm_workspace_file_t;

struct apg_wasm_control {
    uint64_t                     accepted_revision;
    uint64_t                     revision;
    char                        *entry_project;
    apg_wasm_workspace_file_t   *files;
    size_t                       files_len;
    size_t                       files_capacity;
    uc_arena                     arena;
    apg_project_v2_resolved_t    resolved;
    apg_project_v2_compiled_t    compiled;
    uc_arena                     registry_arena;
    apg_v2_registry_t            registry;
    unsigned char               *prepared_image;
    size_t                       prepared_image_size;
    bool                         validated;
    bool                         compiled_ok;
    apg_wasm_diagnostic_t        diagnostic;
    apg_wasm_workspace_summary_t summary;
    char                         diagnostic_phase[32];
    char                         diagnostic_code[32];
    char                         diagnostic_file[APG_WASM_DIAGNOSTIC_TEXT];
    char                         diagnostic_path[APG_WASM_DIAGNOSTIC_TEXT];
    char                         diagnostic_message[APG_WASM_DIAGNOSTIC_TEXT];
};

static char *copy_text(const char *text, size_t text_len) {
    if (!text)
        return NULL;
    char *copy = malloc(text_len + 1u);
    if (!copy)
        return NULL;
    memcpy(copy, text, text_len);
    copy[text_len] = '\0';
    return copy;
}

static void copy_diagnostic_text(char *out, size_t out_size, const char *text) {
    if (!out || out_size == 0u)
        return;
    snprintf(out, out_size, "%s", text ? text : "");
}

static apg_wasm_status_t status_from_uc(uc_status status) {
    switch (status) {
    case UC_OK:
        return APG_WASM_STATUS_OK;
    case UC_E_LEX:
    case UC_E_PARSE:
        return APG_WASM_STATUS_PARSE_ERROR;
    case UC_E_TYPE:
    case UC_E_RANGE:
    case UC_E_MISSING:
        return APG_WASM_STATUS_VALIDATION_ERROR;
    case UC_E_OOM:
        return APG_WASM_STATUS_OUT_OF_MEMORY;
    case UC_E_IO:
        return APG_WASM_STATUS_INVALID_ARGUMENT;
    }
    return APG_WASM_STATUS_RUNTIME_ERROR;
}

static apg_wasm_status_t set_diagnostic(
    apg_wasm_control_t *control,
    apg_wasm_status_t   status,
    const char         *phase,
    const char         *code,
    const char         *file,
    const char         *path,
    const char         *message
) {
    if (!control)
        return status;
    copy_diagnostic_text(control->diagnostic_phase, sizeof(control->diagnostic_phase), phase);
    copy_diagnostic_text(control->diagnostic_code, sizeof(control->diagnostic_code), code);
    copy_diagnostic_text(control->diagnostic_file, sizeof(control->diagnostic_file), file);
    copy_diagnostic_text(control->diagnostic_path, sizeof(control->diagnostic_path), path);
    copy_diagnostic_text(control->diagnostic_message, sizeof(control->diagnostic_message), message);
    control->diagnostic = (apg_wasm_diagnostic_t){
        .revision = control->revision,
        .status   = status,
        .phase    = control->diagnostic_phase,
        .code     = control->diagnostic_code,
        .file     = control->diagnostic_file,
        .path     = control->diagnostic_path,
        .message  = control->diagnostic_message,
    };
    return status;
}

static void clear_workspace(apg_wasm_control_t *control) {
    if (!control)
        return;
    for (size_t i = 0u; i < control->files_len; ++i) {
        free(control->files[i].path);
        free(control->files[i].content);
    }
    free(control->files);
    free(control->entry_project);
    free(control->prepared_image);
    uc_arena_free(&control->registry_arena);
    control->files               = NULL;
    control->files_len           = 0u;
    control->files_capacity      = 0u;
    control->entry_project       = NULL;
    control->prepared_image      = NULL;
    control->prepared_image_size = 0u;
    control->validated           = false;
    control->compiled_ok         = false;
    memset(&control->resolved, 0, sizeof(control->resolved));
    memset(&control->compiled, 0, sizeof(control->compiled));
    memset(&control->registry, 0, sizeof(control->registry));
    memset(&control->summary, 0, sizeof(control->summary));
    uc_arena_reset(&control->arena);
}

static char *normalize_path(const char *path, size_t path_len) {
    if (!path || path_len == 0u || path[0] == '/' || path[0] == '\\')
        return NULL;

    char   *result   = malloc(path_len + 1u);
    size_t *segments = malloc((path_len + 1u) * sizeof(*segments));
    if (!result || !segments) {
        free(result);
        free(segments);
        return NULL;
    }

    size_t result_len   = 0u;
    size_t segments_len = 0u;
    size_t offset       = 0u;
    while (offset < path_len) {
        while (offset < path_len && path[offset] == '/')
            ++offset;
        const size_t start = offset;
        while (offset < path_len && path[offset] != '/') {
            if (path[offset] == '\0' || path[offset] == '\\' || path[offset] == ':') {
                free(result);
                free(segments);
                return NULL;
            }
            ++offset;
        }
        const size_t segment_len = offset - start;
        if (segment_len == 0u || (segment_len == 1u && path[start] == '.'))
            continue;
        if (segment_len == 2u && path[start] == '.' && path[start + 1u] == '.') {
            if (segments_len == 0u) {
                free(result);
                free(segments);
                return NULL;
            }
            result_len = segments[--segments_len];
            continue;
        }
        segments[segments_len++] = result_len;
        if (result_len > 0u)
            result[result_len++] = '/';
        memcpy(result + result_len, path + start, segment_len);
        result_len += segment_len;
    }
    free(segments);
    if (result_len == 0u) {
        free(result);
        return NULL;
    }
    result[result_len] = '\0';
    return result;
}

static char *resolve_path(const char *entry_project, const char *reference) {
    if (!entry_project || !reference)
        return NULL;
    const char  *separator     = strrchr(entry_project, '/');
    const size_t directory_len = separator ? (size_t)(separator - entry_project + 1) : 0u;
    const size_t reference_len = strlen(reference);
    char        *joined        = malloc(directory_len + reference_len + 1u);
    if (!joined)
        return NULL;
    memcpy(joined, entry_project, directory_len);
    memcpy(joined + directory_len, reference, reference_len + 1u);
    char *resolved = normalize_path(joined, directory_len + reference_len);
    free(joined);
    return resolved;
}

static const apg_wasm_workspace_file_t *find_file(const apg_wasm_control_t *control, const char *path) {
    if (!control || !path)
        return NULL;
    for (size_t i = 0u; i < control->files_len; ++i) {
        if (strcmp(control->files[i].path, path) == 0)
            return &control->files[i];
    }
    return NULL;
}

static apg_wasm_status_t report_uc_error(
    apg_wasm_control_t *control, const char *phase, const char *file, const char *path, const uc_error *error
) {
    const uc_status uc = error ? error->status : UC_E_TYPE;
    return set_diagnostic(
        control, status_from_uc(uc), phase, uc_status_str(uc), file, path, error ? error->msg : "unknown APGCore error"
    );
}

static apg_wasm_status_t
report_compile_error(apg_wasm_control_t *control, const char *file, const char *path, const uc_error *error) {
    const uc_status         uc     = error ? error->status : UC_E_TYPE;
    const apg_wasm_status_t status = uc == UC_E_OOM ? APG_WASM_STATUS_OUT_OF_MEMORY : APG_WASM_STATUS_COMPILE_ERROR;
    return set_diagnostic(
        control, status, "compile", uc_status_str(uc), file, path, error ? error->msg : "unknown compiler error"
    );
}

static apg_wasm_status_t load_resolved_workspace(apg_wasm_control_t *control) {
    uc_arena_reset(&control->arena);
    memset(&control->resolved, 0, sizeof(control->resolved));
    memset(&control->compiled, 0, sizeof(control->compiled));
    control->validated   = false;
    control->compiled_ok = false;

    const apg_wasm_workspace_file_t *project_file = find_file(control, control->entry_project);
    if (!project_file || project_file->role != APG_WASM_FILE_PROJECT)
        return set_diagnostic(
            control, APG_WASM_STATUS_INVALID_ARGUMENT, "workspace", "APG_PROJECT_MISSING", control->entry_project, "$",
            "entry project is missing from the workspace"
        );

    uc_error  error = {0};
    uc_status uc    = apg_project_v2_load_string(
        project_file->content, project_file->content_len, &control->arena, &control->resolved.project, &error
    );
    if (uc != UC_OK)
        return report_uc_error(control, "project", project_file->path, "$", &error);

    const size_t                  unit_count = control->resolved.project.units_len;
    apg_project_v2_loaded_unit_t *units = uc_arena_alloc(&control->arena, unit_count * sizeof(*units), sizeof(void *));
    if (!units && unit_count > 0u)
        return set_diagnostic(
            control, APG_WASM_STATUS_OUT_OF_MEMORY, "workspace", "APG_OOM", project_file->path, "$.units",
            "arena OOM while resolving project units"
        );
    if (unit_count > 0u)
        memset(units, 0, unit_count * sizeof(*units));

    for (size_t i = 0u; i < unit_count; ++i) {
        const apg_project_v2_unit_ref_t *reference     = &control->resolved.project.units[i];
        char                            *resolved_path = resolve_path(control->entry_project, reference->file);
        if (!resolved_path)
            return set_diagnostic(
                control, APG_WASM_STATUS_VALIDATION_ERROR, "workspace", "APG_PATH_INVALID", project_file->path,
                "$.units[].file", "unit reference escapes or is invalid for the in-memory workspace"
            );
        const apg_wasm_workspace_file_t *unit_file = find_file(control, resolved_path);
        if (!unit_file || unit_file->role != APG_WASM_FILE_UNIT) {
            apg_wasm_status_t status = set_diagnostic(
                control, APG_WASM_STATUS_VALIDATION_ERROR, "workspace", "APG_UNIT_MISSING", resolved_path,
                "$.units[].file", "referenced unit is missing from the workspace"
            );
            free(resolved_path);
            return status;
        }
        for (size_t previous = 0u; previous < i; ++previous) {
            if (strcmp(units[previous].resolved_path, resolved_path) == 0) {
                apg_wasm_status_t status = set_diagnostic(
                    control, APG_WASM_STATUS_VALIDATION_ERROR, "workspace", "APG_UNIT_DUPLICATE", resolved_path,
                    "$.units[].file", "duplicate resolved unit file"
                );
                free(resolved_path);
                return status;
            }
        }
        char *arena_path = uc_arena_strndup(&control->arena, resolved_path, strlen(resolved_path));
        free(resolved_path);
        if (!arena_path)
            return set_diagnostic(
                control, APG_WASM_STATUS_OUT_OF_MEMORY, "workspace", "APG_OOM", unit_file->path, "$.units",
                "arena OOM while copying resolved unit path"
            );
        units[i].id            = reference->id;
        units[i].file          = reference->file;
        units[i].resolved_path = arena_path;
        uc                     = apg_unit_v2_load_string(
            unit_file->content, unit_file->content_len, &control->arena, &units[i].unit, &error
        );
        if (uc != UC_OK)
            return report_uc_error(control, "unit", unit_file->path, "$", &error);
    }

    control->resolved.units     = units;
    control->resolved.units_len = unit_count;
    uc                          = apg_project_v2_validate_resolved(&control->resolved, &error);
    if (uc != UC_OK)
        return report_uc_error(control, "project", project_file->path, "$.chain", &error);

    control->validated = true;
    control->summary   = (apg_wasm_workspace_summary_t){
          .revision       = control->revision,
          .unit_count     = (uint32_t)unit_count,
          .instance_count = (uint32_t)control->resolved.project.nodes_len,
    };
    return set_diagnostic(control, APG_WASM_STATUS_OK, "validate", "APG_OK", project_file->path, "$", "");
}

uint32_t apg_wasm_control_abi_version(void) { return APG_WASM_ABI_VERSION; }

uint32_t apg_wasm_control_capabilities(void) { return APG_WASM_CAP_WORKSPACE | APG_WASM_CAP_PREPARED_IMAGE; }

apg_wasm_control_t *apg_wasm_control_create(size_t arena_bytes) {
    apg_wasm_control_t *control = calloc(1u, sizeof(*control));
    if (!control)
        return NULL;
    if (uc_arena_init(&control->arena, arena_bytes ? arena_bytes : APG_WASM_DEFAULT_ARENA_BYTES) != 0) {
        free(control);
        return NULL;
    }
    set_diagnostic(control, APG_WASM_STATUS_OK, "idle", "APG_OK", "", "", "");
    return control;
}

void apg_wasm_control_destroy(apg_wasm_control_t *control) {
    if (!control)
        return;
    clear_workspace(control);
    uc_arena_free(&control->arena);
    free(control);
}

apg_wasm_status_t apg_wasm_control_begin_workspace(
    apg_wasm_control_t *control, uint64_t revision, const char *entry_project, size_t entry_project_len
) {
    if (!control || !entry_project || entry_project_len == 0u)
        return APG_WASM_STATUS_INVALID_ARGUMENT;
    if (revision <= control->accepted_revision)
        return set_diagnostic(
            control, APG_WASM_STATUS_STALE_REVISION, "workspace", "APG_STALE_REVISION",
            control->entry_project ? control->entry_project : "", "$",
            "workspace revision is not newer than the accepted revision"
        );

    char *normalized = normalize_path(entry_project, entry_project_len);
    if (!normalized)
        return set_diagnostic(
            control, APG_WASM_STATUS_INVALID_ARGUMENT, "workspace", "APG_PATH_INVALID", "", "$",
            "entry project path must be a confined relative path"
        );
    clear_workspace(control);
    control->revision          = revision;
    control->accepted_revision = revision;
    control->entry_project     = normalized;
    return set_diagnostic(control, APG_WASM_STATUS_OK, "workspace", "APG_OK", normalized, "$", "");
}

apg_wasm_status_t apg_wasm_control_put_file(
    apg_wasm_control_t  *control,
    apg_wasm_file_role_t role,
    const char          *path,
    size_t               path_len,
    const char          *content,
    size_t               content_len
) {
    if (!control || !control->entry_project || !path || !content || content_len == 0u ||
        (role != APG_WASM_FILE_PROJECT && role != APG_WASM_FILE_UNIT))
        return APG_WASM_STATUS_INVALID_ARGUMENT;
    char *normalized = normalize_path(path, path_len);
    if (!normalized)
        return set_diagnostic(
            control, APG_WASM_STATUS_INVALID_ARGUMENT, "workspace", "APG_PATH_INVALID", "", "$",
            "workspace file path must be a confined relative path"
        );
    if (find_file(control, normalized)) {
        apg_wasm_status_t status = set_diagnostic(
            control, APG_WASM_STATUS_INVALID_ARGUMENT, "workspace", "APG_FILE_DUPLICATE", normalized, "$",
            "workspace contains a duplicate file path"
        );
        free(normalized);
        return status;
    }
    char *content_copy = copy_text(content, content_len);
    if (!content_copy) {
        free(normalized);
        return set_diagnostic(
            control, APG_WASM_STATUS_OUT_OF_MEMORY, "workspace", "APG_OOM", "", "$",
            "cannot copy workspace file content"
        );
    }
    if (control->files_len == control->files_capacity) {
        const size_t               capacity = control->files_capacity ? control->files_capacity * 2u : 8u;
        apg_wasm_workspace_file_t *files    = realloc(control->files, capacity * sizeof(*files));
        if (!files) {
            free(normalized);
            free(content_copy);
            return set_diagnostic(
                control, APG_WASM_STATUS_OUT_OF_MEMORY, "workspace", "APG_OOM", "", "$",
                "cannot grow workspace file table"
            );
        }
        control->files          = files;
        control->files_capacity = capacity;
    }
    control->files[control->files_len++] = (apg_wasm_workspace_file_t
    ){.role = role, .path = normalized, .content = content_copy, .content_len = content_len};
    control->validated                   = false;
    control->compiled_ok                 = false;
    return set_diagnostic(control, APG_WASM_STATUS_OK, "workspace", "APG_OK", normalized, "$", "");
}

apg_wasm_status_t apg_wasm_control_validate_workspace(apg_wasm_control_t *control) {
    if (!control || !control->entry_project)
        return APG_WASM_STATUS_INVALID_ARGUMENT;
    return load_resolved_workspace(control);
}

apg_wasm_status_t apg_wasm_control_compile_workspace(apg_wasm_control_t *control) {
    if (!control || !control->entry_project)
        return APG_WASM_STATUS_INVALID_ARGUMENT;
    apg_wasm_status_t status = load_resolved_workspace(control);
    if (status != APG_WASM_STATUS_OK)
        return status;

    uc_error        error = {0};
    const uc_status uc    = apg_project_v2_compile(&control->resolved, &control->arena, &control->compiled, &error);
    if (uc != UC_OK)
        return report_compile_error(control, control->entry_project, "$.chain", &error);
    control->compiled_ok            = true;
    control->summary.node_count     = (uint32_t)control->compiled.plan.nodes_len;
    control->summary.schedule_count = (uint32_t)control->compiled.plan.schedule_len;
    control->summary.signal_count   = (uint32_t)control->compiled.expanded_unit.signals_len;
    control->summary.param_count    = (uint32_t)control->compiled.expanded_unit.params_len;
    return set_diagnostic(control, APG_WASM_STATUS_OK, "compile", "APG_OK", control->entry_project, "$", "");
}

apg_wasm_status_t
apg_wasm_control_prepare_workspace(apg_wasm_control_t *control, const apg_wasm_audio_config_t *config) {
    if (!control || !config || config->revision != control->revision || config->sample_rate == 0u ||
        config->block_frames == 0u)
        return APG_WASM_STATUS_INVALID_ARGUMENT;
    apg_wasm_status_t status = apg_wasm_control_compile_workspace(control);
    if (status != APG_WASM_STATUS_OK)
        return status;

    uc_error        error = {0};
    const uc_status uc    = apg_v2_registry_build_with_growth(
        &control->compiled.plan, config->block_frames, (float)config->sample_rate, &control->registry_arena,
        &control->registry, &error
    );
    if (uc != UC_OK)
        return report_compile_error(control, control->entry_project, "$.registry", &error);

    unsigned char *image      = NULL;
    size_t         image_size = 0u;
    if (!apg_wasm_image_build(&control->registry, control->revision, &image, &image_size, &error))
        return report_compile_error(control, control->entry_project, "$.image", &error);
    free(control->prepared_image);
    control->prepared_image      = image;
    control->prepared_image_size = image_size;
    return set_diagnostic(control, APG_WASM_STATUS_OK, "prepare", "APG_OK", control->entry_project, "$", "");
}

const unsigned char *apg_wasm_control_prepared_image(const apg_wasm_control_t *control, size_t *out_size) {
    if (out_size)
        *out_size = control ? control->prepared_image_size : 0u;
    return control ? control->prepared_image : NULL;
}

uint32_t apg_wasm_control_param_count(const apg_wasm_control_t *control) {
    return control && control->prepared_image && control->registry.params_len <= UINT32_MAX
               ? (uint32_t)control->registry.params_len
               : 0u;
}

const char *apg_wasm_control_param_name(const apg_wasm_control_t *control, uint32_t index) {
    return control && control->prepared_image && index < control->registry.params_len
               ? control->registry.param_names[index]
               : NULL;
}

uint32_t apg_wasm_control_bypass_count(const apg_wasm_control_t *control) {
    return control && control->prepared_image && control->registry.bypassed_instances_len <= UINT32_MAX
               ? (uint32_t)control->registry.bypassed_instances_len
               : 0u;
}

const char *apg_wasm_control_bypass_name(const apg_wasm_control_t *control, uint32_t index) {
    return control && control->prepared_image && index < control->registry.bypassed_instances_len
               ? control->registry.bypass_instances[index].instance_id
               : NULL;
}

const apg_wasm_diagnostic_t *apg_wasm_control_last_diagnostic(const apg_wasm_control_t *control) {
    return control ? &control->diagnostic : NULL;
}

const apg_wasm_workspace_summary_t *apg_wasm_control_workspace_summary(const apg_wasm_control_t *control) {
    return control && control->validated ? &control->summary : NULL;
}
