#include <apgcore/validator/project_v2.h>

#include <apgcore/parser/parser_v2.h>
#include <apgcore/validator/project_validator_v2.h>

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static uc_status set_error(uc_error *err, uc_status status, const char *msg) {
    uc_loc loc = {0, 0};
    uc_error_set(err, status, loc, "%s", msg);
    return status;
}

static bool path_is_absolute(const char *path) { return path && path[0] == '/'; }

static bool path_is_under_root(const char *root, const char *path) {
    if (!root || !path)
        return false;
    if (strcmp(root, "/") == 0)
        return true;
    size_t root_len = strlen(root);
    return strcmp(root, path) == 0 || (strncmp(root, path, root_len) == 0 && path[root_len] == '/');
}

static bool relative_path_escapes_root(const char *project_dir, const char *workspace_root, const char *file) {
    if (!path_is_under_root(workspace_root, project_dir) || !file)
        return true;

    size_t      depth    = 0u;
    size_t      root_len = strlen(workspace_root);
    const char *cursor   = project_dir + root_len;
    while (*cursor) {
        while (*cursor == '/')
            cursor++;
        if (!*cursor)
            break;
        depth++;
        while (*cursor && *cursor != '/')
            cursor++;
    }

    cursor = file;
    while (*cursor) {
        while (*cursor == '/')
            cursor++;
        const char *component = cursor;
        while (*cursor && *cursor != '/')
            cursor++;
        size_t len = (size_t)(cursor - component);
        if (len == 0u || (len == 1u && component[0] == '.'))
            continue;
        if (len == 2u && component[0] == '.' && component[1] == '.') {
            if (depth == 0u)
                return true;
            depth--;
        } else {
            depth++;
        }
    }
    return false;
}

static uc_status arena_strdup_path(uc_arena *arena, const char *path, const char **out, uc_error *err) {
    char *copy = uc_arena_strndup(arena, path, strlen(path));
    if (!copy)
        return set_error(err, UC_E_OOM, "arena OOM");
    *out = copy;
    return UC_OK;
}

static uc_status project_dir_from_realpath(const char *path, char *out_dir, size_t out_dir_size, uc_error *err) {
    char real[PATH_MAX];
    if (!realpath(path, real)) {
        uc_loc loc = {0, 0};
        uc_error_set(err, UC_E_IO, loc, "cannot resolve project path '%s'", path ? path : "");
        return UC_E_IO;
    }

    const char *slash = strrchr(real, '/');
    if (!slash)
        return set_error(err, UC_E_IO, "project path has no directory");

    size_t len = slash == real ? 1u : (size_t)(slash - real);
    if (len + 1u > out_dir_size)
        return set_error(err, UC_E_RANGE, "project directory path is too long");
    memcpy(out_dir, real, len);
    out_dir[len] = '\0';
    return UC_OK;
}

static uc_status resolve_project_unit_path(
    const char  *project_dir,
    const char  *workspace_root,
    const char  *file,
    uc_arena    *arena,
    const char **out_path,
    uc_error    *err
) {
    *out_path = NULL;
    if (!file || file[0] == '\0')
        return set_error(err, UC_E_MISSING, "unit file path is empty");
    if (path_is_absolute(file)) {
        char msg[128];
        snprintf(msg, sizeof(msg), "absolute unit file paths are not allowed: '%s'", file);
        return set_error(err, UC_E_RANGE, msg);
    }
    if (relative_path_escapes_root(project_dir, workspace_root, file)) {
        char msg[128];
        snprintf(msg, sizeof(msg), "unit file '%s' escapes workspace root", file);
        return set_error(err, UC_E_RANGE, msg);
    }

    char candidate[PATH_MAX];
    int  written = snprintf(candidate, sizeof(candidate), "%s/%s", project_dir, file);
    if (written < 0 || (size_t)written >= sizeof(candidate))
        return set_error(err, UC_E_RANGE, "unit file path is too long");

    char real[PATH_MAX];
    if (!realpath(candidate, real)) {
        uc_loc loc = {0, 0};
        uc_error_set(err, UC_E_IO, loc, "cannot resolve unit file '%s'", file);
        return UC_E_IO;
    }
    if (!path_is_under_root(workspace_root, real)) {
        char msg[128];
        snprintf(msg, sizeof(msg), "unit file '%s' escapes workspace root", file);
        return set_error(err, UC_E_RANGE, msg);
    }

    return arena_strdup_path(arena, real, out_path, err);
}

static uc_status
validate_project_unit_path(const char *project_dir, const char *workspace_root, const char *file, uc_error *err) {
    if (!file || file[0] == '\0')
        return set_error(err, UC_E_MISSING, "unit file path is empty");
    if (path_is_absolute(file)) {
        char msg[128];
        snprintf(msg, sizeof(msg), "absolute unit file paths are not allowed: '%s'", file);
        return set_error(err, UC_E_RANGE, msg);
    }
    if (relative_path_escapes_root(project_dir, workspace_root, file)) {
        char msg[128];
        snprintf(msg, sizeof(msg), "unit file '%s' escapes workspace root", file);
        return set_error(err, UC_E_RANGE, msg);
    }
    return UC_OK;
}

static bool resolved_unit_path_seen(const apg_project_v2_loaded_unit_t *units, size_t units_len, const char *path) {
    for (size_t i = 0; i < units_len; i++) {
        if (units[i].resolved_path && path && strcmp(units[i].resolved_path, path) == 0)
            return true;
    }
    return false;
}

static bool unit_ref_is_active(const apg_project_v2_t *project, const char *id) {
    if (!project || !id)
        return false;
    for (size_t i = 0; i < project->nodes_len; i++) {
        if (project->nodes[i].unit && strcmp(project->nodes[i].unit, id) == 0)
            return true;
    }
    return false;
}

uc_status
apg_project_v2_parse_string(const char *src, size_t src_len, uc_arena *arena, uc_node **out_root, uc_error *err) {
    return apg_v2_parse_string(src, src_len, arena, out_root, err);
}

uc_status apg_project_v2_parse_file(const char *path, uc_arena *arena, uc_node **out_root, uc_error *err) {
    return apg_v2_parse_file(path, arena, out_root, err);
}

uc_status
apg_project_v2_load_string(const char *src, size_t src_len, uc_arena *arena, apg_project_v2_t *out, uc_error *err) {
    if (!src || !arena || !out || !err)
        return UC_E_TYPE;
    memset(out, 0, sizeof(*out));
    err->status = UC_OK;

    uc_node  *root   = NULL;
    uc_status status = apg_project_v2_parse_string(src, src_len, arena, &root, err);
    if (status != UC_OK)
        return status;
    return apg_project_v2_validate_root(root, arena, out, err);
}

uc_status apg_project_v2_load_file(const char *path, uc_arena *arena, apg_project_v2_t *out, uc_error *err) {
    if (!arena || !out || !err)
        return UC_E_TYPE;
    memset(out, 0, sizeof(*out));
    err->status = UC_OK;

    uc_node  *root   = NULL;
    uc_status status = apg_project_v2_parse_file(path, arena, &root, err);
    if (status != UC_OK)
        return status;
    return apg_project_v2_validate_root(root, arena, out, err);
}

uc_status
apg_project_v2_load_resolved_file(const char *path, uc_arena *arena, apg_project_v2_resolved_t *out, uc_error *err) {
    if (!path || !arena || !out || !err)
        return UC_E_TYPE;
    memset(out, 0, sizeof(*out));
    err->status = UC_OK;

    uc_status status = apg_project_v2_load_file(path, arena, &out->project, err);
    if (status != UC_OK)
        return status;

    char workspace_root[PATH_MAX];
    if (!getcwd(workspace_root, sizeof(workspace_root)))
        return set_error(err, UC_E_IO, "cannot resolve workspace root");

    char project_real[PATH_MAX];
    if (!realpath(path, project_real)) {
        uc_loc loc = {0, 0};
        uc_error_set(err, UC_E_IO, loc, "cannot resolve project path '%s'", path);
        return UC_E_IO;
    }
    if (!path_is_under_root(workspace_root, project_real))
        return set_error(err, UC_E_RANGE, "project path escapes workspace root");

    char project_dir[PATH_MAX];
    status = project_dir_from_realpath(path, project_dir, sizeof(project_dir), err);
    if (status != UC_OK)
        return status;

    size_t active_units_len = 0u;
    for (size_t i = 0; i < out->project.units_len; i++) {
        if (unit_ref_is_active(&out->project, out->project.units[i].id))
            active_units_len++;
    }

    apg_project_v2_loaded_unit_t *items = uc_arena_alloc(arena, active_units_len * sizeof(*items), sizeof(void *));
    if (!items && active_units_len > 0)
        return set_error(err, UC_E_OOM, "arena OOM");

    size_t loaded_units_len = 0u;
    for (size_t i = 0; i < out->project.units_len; i++) {
        const apg_project_v2_unit_ref_t *ref           = &out->project.units[i];
        const char                      *resolved_path = NULL;
        status = validate_project_unit_path(project_dir, workspace_root, ref->file, err);
        if (status != UC_OK)
            return status;
        if (!unit_ref_is_active(&out->project, ref->id))
            continue;
        status = resolve_project_unit_path(project_dir, workspace_root, ref->file, arena, &resolved_path, err);
        if (status != UC_OK)
            return status;
        if (resolved_unit_path_seen(items, loaded_units_len, resolved_path)) {
            char msg[128];
            snprintf(msg, sizeof(msg), "duplicate resolved unit file '%s'", ref->file ? ref->file : "");
            return set_error(err, UC_E_RANGE, msg);
        }

        items[loaded_units_len].id            = ref->id;
        items[loaded_units_len].file          = ref->file;
        items[loaded_units_len].resolved_path = resolved_path;
        status = apg_unit_v2_load_file(resolved_path, arena, &items[loaded_units_len].unit, err);
        if (status != UC_OK)
            return status;
        loaded_units_len++;
    }

    out->units     = items;
    out->units_len = loaded_units_len;

    status = apg_project_v2_validate_resolved(out, err);
    if (status != UC_OK)
        return status;

    return UC_OK;
}
