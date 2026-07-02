#include <apgcore/project_v2.h>

#include <apgcore/parser_v2.h>
#include <yaml/node.h>

#include <errno.h>
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

static bool scalar_eq(const uc_node *node, const char *expected) {
    return node && node->kind == UC_NODE_SCALAR && strcmp(node->text, expected) == 0;
}

static const char *required_scalar(const uc_node *map, const char *key, const char *path, uc_error *err) {
    const uc_node *node = uc_node_find(map, key);
    if (!node || node->kind != UC_NODE_SCALAR) {
        char msg[128];
        snprintf(msg, sizeof(msg), "missing scalar field '%s'", path ? path : key);
        set_error(err, UC_E_MISSING, msg);
        return NULL;
    }
    return node->text;
}

static const uc_node *required_map(const uc_node *map, const char *key, const char *path, uc_error *err) {
    const uc_node *node = uc_node_find(map, key);
    if (!node || node->kind != UC_NODE_MAP) {
        char msg[128];
        snprintf(msg, sizeof(msg), "missing map field '%s'", path ? path : key);
        set_error(err, UC_E_MISSING, msg);
        return NULL;
    }
    return node;
}

static const uc_node *required_seq(const uc_node *map, const char *key, const char *path, uc_error *err) {
    const uc_node *node = uc_node_find(map, key);
    if (!node || node->kind != UC_NODE_SEQ) {
        char msg[128];
        snprintf(msg, sizeof(msg), "missing sequence field '%s'", path ? path : key);
        set_error(err, UC_E_MISSING, msg);
        return NULL;
    }
    return node;
}

static const uc_node *optional_map(const uc_node *map, const char *key, const char *path, uc_error *err) {
    const uc_node *node = uc_node_find(map, key);
    if (!node)
        return NULL;
    if (node->kind != UC_NODE_MAP) {
        char msg[128];
        snprintf(msg, sizeof(msg), "%s must be a map", path ? path : key);
        set_error(err, UC_E_TYPE, msg);
        return NULL;
    }
    return node;
}

static const uc_node *optional_seq(const uc_node *map, const char *key, const char *path, uc_error *err) {
    const uc_node *node = uc_node_find(map, key);
    if (!node)
        return NULL;
    if (node->kind != UC_NODE_SEQ) {
        char msg[128];
        snprintf(msg, sizeof(msg), "%s must be a sequence", path ? path : key);
        set_error(err, UC_E_TYPE, msg);
        return NULL;
    }
    return node;
}

static uc_value to_value(const uc_node *node) {
    uc_value value = {UC_VAL_LITERAL, ""};
    if (!node)
        return value;
    if (node->kind == UC_NODE_VARREF) {
        value.kind = UC_VAL_VARREF;
        value.text = node->text;
    } else if (node->kind == UC_NODE_SCALAR) {
        value.kind = UC_VAL_LITERAL;
        value.text = node->text;
    }
    return value;
}

static bool profile_is_valid(const char *profile) {
    return profile && (strcmp(profile, "desktop_full") == 0 || strcmp(profile, "wasm_realtime") == 0 ||
                       strcmp(profile, "m7_static") == 0 || strcmp(profile, "offline_render") == 0);
}

static bool unit_ref_exists(const apg_project_v2_t *project, const char *id) {
    for (size_t i = 0; i < project->units_len; i++) {
        if (project->units[i].id && id && strcmp(project->units[i].id, id) == 0)
            return true;
    }
    return false;
}

static bool node_exists(const apg_project_v2_t *project, const char *id) {
    for (size_t i = 0; i < project->nodes_len; i++) {
        if (project->nodes[i].id && id && strcmp(project->nodes[i].id, id) == 0)
            return true;
    }
    return false;
}

static bool endpoint_is_system(const char *endpoint) {
    return endpoint && (strcmp(endpoint, "system.input") == 0 || strcmp(endpoint, "system.output") == 0);
}

static bool endpoint_node_id(const char *endpoint, char *buf, size_t buf_size) {
    if (!endpoint || !buf || buf_size == 0)
        return false;
    const char *dot = strchr(endpoint, '.');
    if (!dot || dot == endpoint || dot[1] == '\0' || strchr(dot + 1, '.'))
        return false;
    size_t len = (size_t)(dot - endpoint);
    if (len >= buf_size)
        return false;
    memcpy(buf, endpoint, len);
    buf[len] = '\0';
    return true;
}

static uc_status
validate_endpoint(const apg_project_v2_t *project, const char *endpoint, const char *path, uc_error *err) {
    if (endpoint_is_system(endpoint))
        return UC_OK;
    char node_id[64];
    if (!endpoint_node_id(endpoint, node_id, sizeof(node_id)) || !node_exists(project, node_id)) {
        char msg[128];
        snprintf(msg, sizeof(msg), "%s references unknown endpoint '%s'", path, endpoint ? endpoint : "");
        return set_error(err, UC_E_MISSING, msg);
    }
    return UC_OK;
}

static uc_status fill_param_overrides(
    const uc_node *params, uc_arena *arena, apg_project_v2_param_override_t **out_params, size_t *out_len, uc_error *err
) {
    *out_params = NULL;
    *out_len    = 0;
    if (!params)
        return UC_OK;
    if (params->kind != UC_NODE_MAP)
        return set_error(err, UC_E_TYPE, "params must be a map");

    apg_project_v2_param_override_t *items = uc_arena_alloc(arena, params->map_len * sizeof(*items), sizeof(void *));
    if (!items && params->map_len > 0)
        return set_error(err, UC_E_OOM, "arena OOM");

    for (size_t i = 0; i < params->map_len; i++) {
        for (size_t j = 0; j < i; j++) {
            if (strcmp(params->map[j].key, params->map[i].key) == 0) {
                char msg[128];
                snprintf(
                    msg, sizeof(msg), "duplicate param override '%s'", params->map[i].key ? params->map[i].key : ""
                );
                return set_error(err, UC_E_RANGE, msg);
            }
        }
        if (!params->map[i].value ||
            (params->map[i].value->kind != UC_NODE_SCALAR && params->map[i].value->kind != UC_NODE_VARREF))
            return set_error(err, UC_E_TYPE, "param override value must be scalar");
        items[i].key   = params->map[i].key;
        items[i].value = to_value(params->map[i].value);
    }

    *out_params = items;
    *out_len    = params->map_len;
    return UC_OK;
}

static uc_status fill_units(const uc_node *units, uc_arena *arena, apg_project_v2_t *out, uc_error *err) {
    if (!units || units->kind != UC_NODE_SEQ)
        return set_error(err, UC_E_MISSING, "missing sequence field 'units'");
    if (units->seq_len == 0)
        return set_error(err, UC_E_MISSING, "units must contain at least one unit ref");

    apg_project_v2_unit_ref_t *items = uc_arena_alloc(arena, units->seq_len * sizeof(*items), sizeof(void *));
    if (!items)
        return set_error(err, UC_E_OOM, "arena OOM");

    for (size_t i = 0; i < units->seq_len; i++) {
        const uc_node *unit = units->seq[i];
        if (!unit || unit->kind != UC_NODE_MAP)
            return set_error(err, UC_E_TYPE, "unit ref must be a map");
        const char *id = required_scalar(unit, "id", "units[].id", err);
        if (!id)
            return err->status;
        for (size_t j = 0; j < i; j++) {
            if (strcmp(items[j].id, id) == 0) {
                char msg[128];
                snprintf(msg, sizeof(msg), "duplicate unit ref id '%s'", id);
                return set_error(err, UC_E_RANGE, msg);
            }
        }
        const char *file = required_scalar(unit, "file", "units[].file", err);
        if (!file)
            return err->status;
        items[i].id   = id;
        items[i].file = file;
    }

    out->units     = items;
    out->units_len = units->seq_len;
    return UC_OK;
}

static uc_status fill_nodes(const uc_node *nodes, uc_arena *arena, apg_project_v2_t *out, uc_error *err) {
    if (!nodes || nodes->kind != UC_NODE_SEQ)
        return set_error(err, UC_E_MISSING, "missing sequence field 'chain.nodes'");
    if (nodes->seq_len == 0)
        return set_error(err, UC_E_MISSING, "chain.nodes must contain at least one node");

    apg_project_v2_node_t *items = uc_arena_alloc(arena, nodes->seq_len * sizeof(*items), sizeof(void *));
    if (!items)
        return set_error(err, UC_E_OOM, "arena OOM");

    for (size_t i = 0; i < nodes->seq_len; i++) {
        const uc_node *node = nodes->seq[i];
        if (!node || node->kind != UC_NODE_MAP)
            return set_error(err, UC_E_TYPE, "chain node must be a map");
        const char *id = required_scalar(node, "id", "chain.nodes[].id", err);
        if (!id)
            return err->status;
        for (size_t j = 0; j < i; j++) {
            if (strcmp(items[j].id, id) == 0) {
                char msg[128];
                snprintf(msg, sizeof(msg), "duplicate chain node id '%s'", id);
                return set_error(err, UC_E_RANGE, msg);
            }
        }
        const char *unit = required_scalar(node, "unit", "chain.nodes[].unit", err);
        if (!unit)
            return err->status;
        if (!unit_ref_exists(out, unit)) {
            char msg[128];
            snprintf(msg, sizeof(msg), "chain node '%s' references unknown unit '%s'", id, unit);
            return set_error(err, UC_E_MISSING, msg);
        }

        items[i].id   = id;
        items[i].unit = unit;
        uc_status status =
            fill_param_overrides(uc_node_find(node, "params"), arena, &items[i].params, &items[i].params_len, err);
        if (status != UC_OK)
            return status;
    }

    out->nodes     = items;
    out->nodes_len = nodes->seq_len;
    return UC_OK;
}

static uc_status fill_routes(const uc_node *routes, uc_arena *arena, apg_project_v2_t *out, uc_error *err) {
    if (!routes || routes->kind != UC_NODE_SEQ)
        return set_error(err, UC_E_MISSING, "missing sequence field 'chain.routes'");
    if (routes->seq_len == 0)
        return set_error(err, UC_E_MISSING, "chain.routes must contain at least one route");

    apg_project_v2_route_t *items = uc_arena_alloc(arena, routes->seq_len * sizeof(*items), sizeof(void *));
    if (!items)
        return set_error(err, UC_E_OOM, "arena OOM");

    for (size_t i = 0; i < routes->seq_len; i++) {
        const uc_node *route = routes->seq[i];
        if (!route || route->kind != UC_NODE_MAP)
            return set_error(err, UC_E_TYPE, "chain route must be a map");
        const char *from = required_scalar(route, "from", "chain.routes[].from", err);
        if (!from)
            return err->status;
        const char *to = required_scalar(route, "to", "chain.routes[].to", err);
        if (!to)
            return err->status;
        uc_status status = validate_endpoint(out, from, "chain.routes[].from", err);
        if (status != UC_OK)
            return status;
        status = validate_endpoint(out, to, "chain.routes[].to", err);
        if (status != UC_OK)
            return status;
        items[i].from = from;
        items[i].to   = to;
    }

    out->routes     = items;
    out->routes_len = routes->seq_len;
    return UC_OK;
}

static uc_status fill_scenes(const uc_node *scenes, uc_arena *arena, apg_project_v2_t *out, uc_error *err) {
    if (!scenes) {
        out->scenes     = NULL;
        out->scenes_len = 0;
        return UC_OK;
    }
    if (scenes->kind != UC_NODE_SEQ)
        return set_error(err, UC_E_TYPE, "scenes must be a sequence");

    apg_project_v2_scene_t *items = uc_arena_alloc(arena, scenes->seq_len * sizeof(*items), sizeof(void *));
    if (!items && scenes->seq_len > 0)
        return set_error(err, UC_E_OOM, "arena OOM");

    for (size_t i = 0; i < scenes->seq_len; i++) {
        const uc_node *scene = scenes->seq[i];
        if (!scene || scene->kind != UC_NODE_MAP)
            return set_error(err, UC_E_TYPE, "scene must be a map");
        const char *name = required_scalar(scene, "name", "scenes[].name", err);
        if (!name)
            return err->status;
        for (size_t j = 0; j < i; j++) {
            if (strcmp(items[j].name, name) == 0) {
                char msg[128];
                snprintf(msg, sizeof(msg), "duplicate scene name '%s'", name);
                return set_error(err, UC_E_RANGE, msg);
            }
        }
        items[i].name = name;
        uc_status status =
            fill_param_overrides(uc_node_find(scene, "params"), arena, &items[i].params, &items[i].params_len, err);
        if (status != UC_OK)
            return status;
        for (size_t p = 0; p < items[i].params_len; p++) {
            char node_id[64];
            if (!endpoint_node_id(items[i].params[p].key, node_id, sizeof(node_id)) || !node_exists(out, node_id)) {
                char msg[128];
                snprintf(msg, sizeof(msg), "scene '%s' references unknown param '%s'", name, items[i].params[p].key);
                return set_error(err, UC_E_MISSING, msg);
            }
        }
    }

    out->scenes     = items;
    out->scenes_len = scenes->seq_len;
    return UC_OK;
}

static uc_status fill_targets(const uc_node *targets, uc_arena *arena, apg_project_v2_t *out, uc_error *err) {
    if (!targets || targets->kind != UC_NODE_MAP)
        return set_error(err, UC_E_MISSING, "missing map field 'targets'");

    const char *default_profile = required_scalar(targets, "default", "targets.default", err);
    if (!default_profile)
        return err->status;
    if (!profile_is_valid(default_profile)) {
        char msg[128];
        snprintf(msg, sizeof(msg), "targets.default profile '%s' is unknown", default_profile);
        return set_error(err, UC_E_TYPE, msg);
    }
    out->targets.default_profile = default_profile;

    const uc_node *export = optional_seq(targets, "export", "targets.export", err);
    if (!export)
        return err->status == UC_OK ? UC_OK : err->status;

    const char **profiles = uc_arena_alloc(arena, export->seq_len * sizeof(*profiles), sizeof(void *));
    if (!profiles && export->seq_len > 0)
        return set_error(err, UC_E_OOM, "arena OOM");
    for (size_t i = 0; i < export->seq_len; i++) {
        const char *profile = export->seq[i] && export->seq[i]->kind == UC_NODE_SCALAR ? export->seq[i]->text : NULL;
        if (!profile)
            return set_error(err, UC_E_TYPE, "targets.export entries must be scalar");
        if (!profile_is_valid(profile)) {
            char msg[128];
            snprintf(msg, sizeof(msg), "targets.export profile '%s' is unknown", profile);
            return set_error(err, UC_E_TYPE, msg);
        }
        for (size_t j = 0; j < i; j++) {
            if (strcmp(profiles[j], profile) == 0) {
                char msg[128];
                snprintf(msg, sizeof(msg), "duplicate targets.export profile '%s'", profile);
                return set_error(err, UC_E_RANGE, msg);
            }
        }
        profiles[i] = profile;
    }
    out->targets.export_profiles     = profiles;
    out->targets.export_profiles_len = export->seq_len;
    return UC_OK;
}

static uc_status validate_project_root(const uc_node *root, uc_arena *arena, apg_project_v2_t *out, uc_error *err) {
    if (!root || root->kind != UC_NODE_MAP)
        return set_error(err, UC_E_TYPE, "root must be a map");
    if (!scalar_eq(uc_node_find(root, "kind"), "apg.project"))
        return set_error(err, UC_E_TYPE, "kind must be 'apg.project'");
    if (!scalar_eq(uc_node_find(root, "schema"), "apg.project.v2"))
        return set_error(err, UC_E_TYPE, "schema must be 'apg.project.v2'");

    out->name = required_scalar(root, "name", "name", err);
    if (!out->name)
        return err->status;
    out->version = required_scalar(root, "version", "version", err);
    if (!out->version)
        return err->status;

    uc_status status = fill_units(uc_node_find(root, "units"), arena, out, err);
    if (status != UC_OK)
        return status;

    const uc_node *chain = required_map(root, "chain", "chain", err);
    if (!chain)
        return err->status;
    status = fill_nodes(uc_node_find(chain, "nodes"), arena, out, err);
    if (status != UC_OK)
        return status;
    status = fill_routes(uc_node_find(chain, "routes"), arena, out, err);
    if (status != UC_OK)
        return status;
    status = fill_scenes(uc_node_find(root, "scenes"), arena, out, err);
    if (status != UC_OK)
        return status;
    return fill_targets(uc_node_find(root, "targets"), arena, out, err);
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

static bool resolved_unit_path_seen(const apg_project_v2_loaded_unit_t *units, size_t units_len, const char *path) {
    for (size_t i = 0; i < units_len; i++) {
        if (units[i].resolved_path && path && strcmp(units[i].resolved_path, path) == 0)
            return true;
    }
    return false;
}

uc_status
apg_project_v2_load_string(const char *src, size_t src_len, uc_arena *arena, apg_project_v2_t *out, uc_error *err) {
    if (!src || !arena || !out || !err)
        return UC_E_TYPE;
    memset(out, 0, sizeof(*out));
    err->status = UC_OK;

    uc_node  *root   = NULL;
    uc_status status = apg_v2_parse_string(src, src_len, arena, &root, err);
    if (status != UC_OK)
        return status;
    return validate_project_root(root, arena, out, err);
}

uc_status apg_project_v2_load_file(const char *path, uc_arena *arena, apg_project_v2_t *out, uc_error *err) {
    if (!arena || !out || !err)
        return UC_E_TYPE;
    memset(out, 0, sizeof(*out));
    err->status = UC_OK;

    uc_node  *root   = NULL;
    uc_status status = apg_v2_parse_file(path, arena, &root, err);
    if (status != UC_OK)
        return status;
    return validate_project_root(root, arena, out, err);
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

    apg_project_v2_loaded_unit_t *items =
        uc_arena_alloc(arena, out->project.units_len * sizeof(*items), sizeof(void *));
    if (!items && out->project.units_len > 0)
        return set_error(err, UC_E_OOM, "arena OOM");

    for (size_t i = 0; i < out->project.units_len; i++) {
        const apg_project_v2_unit_ref_t *ref           = &out->project.units[i];
        const char                      *resolved_path = NULL;
        status = resolve_project_unit_path(project_dir, workspace_root, ref->file, arena, &resolved_path, err);
        if (status != UC_OK)
            return status;
        if (resolved_unit_path_seen(items, i, resolved_path)) {
            char msg[128];
            snprintf(msg, sizeof(msg), "duplicate resolved unit file '%s'", ref->file ? ref->file : "");
            return set_error(err, UC_E_RANGE, msg);
        }

        items[i].id            = ref->id;
        items[i].file          = ref->file;
        items[i].resolved_path = resolved_path;
        status                 = apg_unit_v2_load_file(resolved_path, arena, &items[i].unit, err);
        if (status != UC_OK)
            return status;
    }

    out->units     = items;
    out->units_len = out->project.units_len;
    return UC_OK;
}
