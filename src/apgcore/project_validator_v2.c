#include <apgcore/project_validator_v2.h>

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

#define APG_PROJECT_SYSTEM_INPUT  "system.input"
#define APG_PROJECT_SYSTEM_OUTPUT "system.output"

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

static const apg_project_v2_node_t *find_project_node(const apg_project_v2_resolved_t *project, const char *id) {
    if (!project)
        return NULL;
    for (size_t i = 0; i < project->project.nodes_len; i++) {
        if (project->project.nodes[i].id && strcmp(project->project.nodes[i].id, id) == 0)
            return &project->project.nodes[i];
    }
    return NULL;
}

static const apg_project_v2_loaded_unit_t *find_loaded_unit(const apg_project_v2_resolved_t *project, const char *id) {
    if (!project)
        return NULL;
    for (size_t i = 0; i < project->units_len; i++) {
        if (project->units[i].id && strcmp(project->units[i].id, id) == 0)
            return &project->units[i];
    }
    return NULL;
}

static bool param_exists(const apg_unit_v2_t *unit, const char *name) {
    if (!unit || !name)
        return false;
    for (size_t i = 0; i < unit->params_len; i++) {
        if (unit->params[i].name && strcmp(unit->params[i].name, name) == 0)
            return true;
    }
    return false;
}

static bool port_is_mono_audio(const apg_unit_v2_port_t *port) {
    return port && port->type && strcmp(port->type, "audio") == 0 && port->channels &&
           strcmp(port->channels, "1") == 0 && port->signals_len <= 1u;
}

static const apg_unit_v2_port_t *find_port(const apg_unit_v2_port_t *ports, size_t ports_len, const char *name) {
    if (!ports || !name)
        return NULL;
    for (size_t i = 0; i < ports_len; i++) {
        if (ports[i].name && strcmp(ports[i].name, name) == 0)
            return &ports[i];
    }
    return NULL;
}

static uc_status validate_node_port(
    const apg_project_v2_resolved_t *project, const char *node_id, const char *port_name, bool output, uc_error *err
) {
    const apg_project_v2_node_t        *node = find_project_node(project, node_id);
    const apg_project_v2_loaded_unit_t *unit = node ? find_loaded_unit(project, node->unit) : NULL;
    if (!node || !unit) {
        char msg[160];
        snprintf(msg, sizeof(msg), "route endpoint references unknown node '%s'", node_id ? node_id : "");
        return set_error(err, UC_E_MISSING, msg);
    }

    const apg_unit_v2_port_t *port = output ? find_port(unit->unit.output_ports, unit->unit.output_ports_len, port_name)
                                            : find_port(unit->unit.input_ports, unit->unit.input_ports_len, port_name);
    if (!port) {
        char msg[192];
        snprintf(
            msg, sizeof(msg), "route endpoint '%s.%s' references unknown %s port", node_id ? node_id : "",
            port_name ? port_name : "", output ? "output" : "input"
        );
        return set_error(err, UC_E_MISSING, msg);
    }
    if (!port_is_mono_audio(port)) {
        char msg[192];
        snprintf(
            msg, sizeof(msg), "route endpoint '%s.%s' must be a mono audio port", node_id ? node_id : "",
            port_name ? port_name : ""
        );
        return set_error(err, UC_E_TYPE, msg);
    }
    return UC_OK;
}

static bool parse_endpoint(const char *endpoint, char *node, size_t node_size, char *port, size_t port_size) {
    if (!endpoint || !node || !port || node_size == 0u || port_size == 0u)
        return false;
    const char *dot = strchr(endpoint, '.');
    if (!dot || dot == endpoint || dot[1] == '\0' || strchr(dot + 1, '.'))
        return false;
    size_t node_len = (size_t)(dot - endpoint);
    size_t port_len = strlen(dot + 1);
    if (node_len >= node_size || port_len >= port_size)
        return false;
    memcpy(node, endpoint, node_len);
    node[node_len] = '\0';
    memcpy(port, dot + 1, port_len + 1u);
    return true;
}

static uc_status
validate_route_endpoint(const apg_project_v2_resolved_t *project, const char *endpoint, bool output, uc_error *err) {
    if (output && strcmp(endpoint, APG_PROJECT_SYSTEM_INPUT) == 0)
        return UC_OK;
    if (!output && strcmp(endpoint, APG_PROJECT_SYSTEM_OUTPUT) == 0)
        return UC_OK;
    if (strcmp(endpoint, APG_PROJECT_SYSTEM_INPUT) == 0 || strcmp(endpoint, APG_PROJECT_SYSTEM_OUTPUT) == 0) {
        char msg[160];
        snprintf(msg, sizeof(msg), "route endpoint '%s' is not valid in this direction", endpoint ? endpoint : "");
        return set_error(err, UC_E_TYPE, msg);
    }

    char node_id[64];
    char port_name[64];
    if (!parse_endpoint(endpoint, node_id, sizeof(node_id), port_name, sizeof(port_name))) {
        char msg[160];
        snprintf(msg, sizeof(msg), "route endpoint '%s' is invalid", endpoint ? endpoint : "");
        return set_error(err, UC_E_TYPE, msg);
    }
    return validate_node_port(project, node_id, port_name, output, err);
}

static uc_status validate_project_routes(const apg_project_v2_resolved_t *project, uc_error *err) {
    if (!project)
        return UC_OK;
    size_t system_input_routes  = 0u;
    size_t system_output_routes = 0u;
    for (size_t i = 0; i < project->project.routes_len; i++) {
        const apg_project_v2_route_t *route  = &project->project.routes[i];
        uc_status                     status = validate_route_endpoint(project, route->from, true, err);
        if (status != UC_OK)
            return status;
        status = validate_route_endpoint(project, route->to, false, err);
        if (status != UC_OK)
            return status;
        if (strcmp(route->from, APG_PROJECT_SYSTEM_INPUT) == 0)
            system_input_routes++;
        if (strcmp(route->to, APG_PROJECT_SYSTEM_OUTPUT) == 0)
            system_output_routes++;
        for (size_t j = 0; j < i; j++) {
            if (strcmp(project->project.routes[j].to, route->to) == 0) {
                char msg[160];
                snprintf(msg, sizeof(msg), "route target '%s' has multiple sources", route->to ? route->to : "");
                return set_error(err, UC_E_RANGE, msg);
            }
        }
    }
    if (system_input_routes == 0u)
        return set_error(err, UC_E_MISSING, "project requires at least one route from system.input");
    if (system_output_routes != 1u)
        return set_error(err, UC_E_MISSING, "project requires exactly one route to system.output");
    return UC_OK;
}

static uc_status validate_param_overrides(const apg_project_v2_resolved_t *project, uc_error *err) {
    for (size_t i = 0; i < project->project.nodes_len; i++) {
        const apg_project_v2_node_t        *node = &project->project.nodes[i];
        const apg_project_v2_loaded_unit_t *unit = node ? find_loaded_unit(project, node->unit) : NULL;
        if (!unit)
            return set_error(err, UC_E_MISSING, "project node references missing loaded unit");
        for (size_t p = 0; p < node->params_len; p++) {
            if (!param_exists(&unit->unit, node->params[p].key)) {
                char msg[192];
                snprintf(
                    msg, sizeof(msg), "project node '%s' overrides unknown param '%s'", node->id ? node->id : "",
                    node->params[p].key ? node->params[p].key : ""
                );
                return set_error(err, UC_E_MISSING, msg);
            }
            if (node->params[p].value.kind != UC_VAL_LITERAL) {
                char msg[192];
                snprintf(
                    msg, sizeof(msg), "project node '%s' param override '%s' must be literal", node->id ? node->id : "",
                    node->params[p].key ? node->params[p].key : ""
                );
                return set_error(err, UC_E_TYPE, msg);
            }
        }
    }
    return UC_OK;
}

static uc_status validate_resolved_routes_and_overrides(const apg_project_v2_resolved_t *project, uc_error *err) {
    uc_status status = validate_project_routes(project, err);
    if (status != UC_OK)
        return status;
    return validate_param_overrides(project, err);
}

uc_status apg_project_v2_validate_resolved(const apg_project_v2_resolved_t *project, uc_error *err) {
    if (!project || !err)
        return UC_E_TYPE;
    return validate_resolved_routes_and_overrides(project, err);
}

uc_status apg_project_v2_validate_root(const uc_node *root, uc_arena *arena, apg_project_v2_t *out, uc_error *err) {
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
