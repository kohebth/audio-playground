#include <apgcore/compiler/project_compiler_v2.h>
#include <apgcore/validator/project_validator_v2.h>

#include <yaml/node.h>

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define APG_PROJECT_SYSTEM_INPUT  "system.input"
#define APG_PROJECT_SYSTEM_OUTPUT "system.output"

static uc_status set_error(uc_error *err, uc_status status, const char *msg) {
    uc_loc loc = {0, 0};
    uc_error_set(err, status, loc, "%s", msg);
    return status;
}

static char *arena_strdup(uc_arena *arena, const char *text, uc_error *err) {
    if (!text)
        text = "";
    char *copy = uc_arena_strndup(arena, text, strlen(text));
    if (!copy)
        set_error(err, UC_E_OOM, "arena OOM");
    return copy;
}

static char *arena_join_dot(uc_arena *arena, const char *left, const char *right, uc_error *err) {
    char   temp[256];
    int    written = snprintf(temp, sizeof(temp), "%s.%s", left ? left : "", right ? right : "");
    size_t len     = written > 0 ? (size_t)written : 0u;
    if (written < 0 || len >= sizeof(temp)) {
        set_error(err, UC_E_RANGE, "project compiler generated name is too long");
        return NULL;
    }
    return arena_strdup(arena, temp, err);
}

static const apg_project_v2_loaded_unit_t *find_loaded_unit(const apg_project_v2_resolved_t *project, const char *id) {
    if (!project || !id)
        return NULL;
    for (size_t i = 0; i < project->units_len; i++) {
        if (project->units[i].id && strcmp(project->units[i].id, id) == 0)
            return &project->units[i];
    }
    return NULL;
}

static const char *port_signal_name(const apg_unit_v2_port_t *port) {
    if (!port)
        return NULL;
    return port->signals_len == 1u ? port->signals[0] : port->name;
}

static bool port_is_mono_audio(const apg_unit_v2_port_t *port) {
    return port && port->type && strcmp(port->type, "audio") == 0 && port->channels &&
           strcmp(port->channels, "1") == 0 && port->signals_len <= 1u;
}

static const char *endpoint_for_node_port(uc_arena *arena, const char *node_id, const char *port_name, uc_error *err) {
    return arena_join_dot(arena, node_id, port_name, err);
}

static const apg_project_v2_route_t *find_route_to(const apg_project_v2_resolved_t *project, const char *endpoint) {
    for (size_t i = 0; i < project->project.routes_len; i++) {
        if (project->project.routes[i].to && strcmp(project->project.routes[i].to, endpoint) == 0)
            return &project->project.routes[i];
    }
    return NULL;
}

static const apg_project_v2_route_t *find_route_from(const apg_project_v2_resolved_t *project, const char *endpoint) {
    for (size_t i = 0; i < project->project.routes_len; i++) {
        if (project->project.routes[i].from && strcmp(project->project.routes[i].from, endpoint) == 0)
            return &project->project.routes[i];
    }
    return NULL;
}

static const char *project_output_signal(const apg_project_v2_resolved_t *project) {
    for (size_t i = 0; i < project->project.routes_len; i++) {
        if (project->project.routes[i].to && strcmp(project->project.routes[i].to, APG_PROJECT_SYSTEM_OUTPUT) == 0) {
            if (strcmp(project->project.routes[i].from, APG_PROJECT_SYSTEM_INPUT) == 0)
                return &APG_PROJECT_SYSTEM_INPUT[7];
            return project->project.routes[i].from;
        }
    }
    return NULL;
}

static const char *remap_signal(
    const apg_project_v2_resolved_t *project,
    const apg_project_v2_node_t     *node,
    const apg_unit_v2_t             *unit,
    const char                      *signal,
    uc_arena                        *arena,
    uc_error                        *err
) {
    for (size_t i = 0; i < unit->input_ports_len; i++) {
        const apg_unit_v2_port_t *port = &unit->input_ports[i];
        if (!port_is_mono_audio(port) || !port_signal_name(port) || strcmp(port_signal_name(port), signal) != 0)
            continue;
        const char                   *endpoint = endpoint_for_node_port(arena, node->id, port->name, err);
        const apg_project_v2_route_t *route    = endpoint ? find_route_to(project, endpoint) : NULL;
        if (!endpoint || err->status != UC_OK)
            return NULL;
        if (!route)
            return endpoint;
        if (strcmp(route->from, APG_PROJECT_SYSTEM_INPUT) == 0)
            return &APG_PROJECT_SYSTEM_INPUT[7];
        return route->from;
    }

    for (size_t i = 0; i < unit->output_ports_len; i++) {
        const apg_unit_v2_port_t *port = &unit->output_ports[i];
        if (!port_is_mono_audio(port) || !port_signal_name(port) || strcmp(port_signal_name(port), signal) != 0)
            continue;
        const char *endpoint = endpoint_for_node_port(arena, node->id, port->name, err);
        if (!endpoint || err->status != UC_OK)
            return NULL;
        if (find_route_from(project, endpoint))
            return endpoint;
        return endpoint;
    }

    return arena_join_dot(arena, node->id, signal, err);
}

static bool signal_seen(const char **signals, size_t signals_len, const char *signal) {
    for (size_t i = 0; i < signals_len; i++) {
        if (signals[i] && signal && strcmp(signals[i], signal) == 0)
            return true;
    }
    return false;
}

static uc_status add_signal(const char ***signals, size_t *signals_len, const char *signal) {
    if (signal_seen(*signals, *signals_len, signal))
        return UC_OK;
    (*signals)[(*signals_len)++] = signal;
    return UC_OK;
}

static const char *namespace_param_ref(uc_arena *arena, const char *instance_id, const char *text, uc_error *err) {
    const char *param = NULL;
    if (text && strncmp(text, "params.", 7) == 0)
        param = text + 7;
    else if (text && strncmp(text, "${params.", 9) == 0) {
        param = text + 9;
    }
    if (!param)
        return text;

    char   temp[256];
    size_t len = strlen(param);
    if (len > 0u && param[len - 1u] == '}')
        len--;
    int written = snprintf(temp, sizeof(temp), "params.%s.%.*s", instance_id ? instance_id : "", (int)len, param);
    if (written < 0 || (size_t)written >= sizeof(temp)) {
        set_error(err, UC_E_RANGE, "project compiler generated param ref is too long");
        return NULL;
    }
    return arena_strdup(arena, temp, err);
}

static uc_node *clone_signal_seq_node(
    const apg_project_v2_resolved_t *project,
    const apg_project_v2_node_t     *project_node,
    const apg_unit_v2_t             *unit,
    const uc_node                   *src,
    uc_arena                        *arena,
    uc_error                        *err
) {
    if (!src || src->kind != UC_NODE_SEQ)
        return NULL;
    uc_node *copy = uc_arena_alloc(arena, sizeof(*copy), sizeof(void *));
    if (!copy) {
        set_error(err, UC_E_OOM, "arena OOM");
        return NULL;
    }
    memset(copy, 0, sizeof(*copy));
    copy->kind    = UC_NODE_SEQ;
    copy->seq_len = src->seq_len;
    copy->seq_cap = src->seq_len;
    copy->seq     = uc_arena_alloc(arena, src->seq_len * sizeof(*copy->seq), sizeof(void *));
    if (!copy->seq && src->seq_len > 0u) {
        set_error(err, UC_E_OOM, "arena OOM");
        return NULL;
    }

    for (size_t i = 0; i < src->seq_len; i++) {
        if (!src->seq[i] || src->seq[i]->kind != UC_NODE_SCALAR) {
            set_error(err, UC_E_TYPE, "project compiler only supports scalar signal arrays");
            return NULL;
        }
        uc_node *item = uc_arena_alloc(arena, sizeof(*item), sizeof(void *));
        if (!item) {
            set_error(err, UC_E_OOM, "arena OOM");
            return NULL;
        }
        memset(item, 0, sizeof(*item));
        item->kind = UC_NODE_SCALAR;
        item->text = remap_signal(project, project_node, unit, src->seq[i]->text, arena, err);
        if (!item->text || err->status != UC_OK)
            return NULL;
        item->text_len = (int)strlen(item->text);
        copy->seq[i]   = item;
    }
    return copy;
}

static uc_status clone_bindings(
    const apg_project_v2_resolved_t *project,
    const apg_project_v2_node_t     *project_node,
    const apg_unit_v2_t             *unit,
    const apg_unit_v2_binding_t     *src,
    size_t                           src_len,
    bool                             signal_section,
    uc_arena                        *arena,
    apg_unit_v2_binding_t          **out,
    size_t                          *out_len,
    uc_error                        *err
) {
    *out     = NULL;
    *out_len = 0;
    if (src_len == 0u)
        return UC_OK;
    apg_unit_v2_binding_t *items = uc_arena_alloc(arena, src_len * sizeof(*items), sizeof(void *));
    if (!items)
        return set_error(err, UC_E_OOM, "arena OOM");

    for (size_t i = 0; i < src_len; i++) {
        items[i] = src[i];
        if (signal_section && src[i].node && src[i].node->kind == UC_NODE_SEQ) {
            items[i].node = clone_signal_seq_node(project, project_node, unit, src[i].node, arena, err);
            if (!items[i].node || err->status != UC_OK)
                return err->status;
            items[i].value.kind = APG_V2_VALUE_LITERAL;
            items[i].value.text = "";
            continue;
        }
        if (signal_section && src[i].value.kind == APG_V2_VALUE_LITERAL) {
            items[i].value.text = remap_signal(project, project_node, unit, src[i].value.text, arena, err);
            if (!items[i].value.text || err->status != UC_OK)
                return err->status;
            items[i].node = NULL;
        } else if (src[i].value.kind == APG_V2_VALUE_VARREF) {
            items[i].value.text = namespace_param_ref(arena, project_node->id, src[i].value.text, err);
            if (!items[i].value.text || err->status != UC_OK)
                return err->status;
            items[i].node = NULL;
        }
    }

    *out     = items;
    *out_len = src_len;
    return UC_OK;
}

static uc_status
build_expanded_params(const apg_project_v2_resolved_t *project, uc_arena *arena, apg_unit_v2_t *unit, uc_error *err) {
    size_t params_len = 0;
    for (size_t i = 0; i < project->project.nodes_len; i++) {
        const apg_project_v2_loaded_unit_t *loaded = find_loaded_unit(project, project->project.nodes[i].unit);
        params_len += loaded ? loaded->unit.params_len : 0u;
    }

    apg_unit_v2_param_t *params = uc_arena_alloc(arena, params_len * sizeof(*params), sizeof(void *));
    if (!params && params_len > 0u)
        return set_error(err, UC_E_OOM, "arena OOM");

    size_t out_index = 0;
    for (size_t i = 0; i < project->project.nodes_len; i++) {
        const apg_project_v2_node_t        *project_node = &project->project.nodes[i];
        const apg_project_v2_loaded_unit_t *loaded       = find_loaded_unit(project, project_node->unit);
        if (!loaded)
            return set_error(err, UC_E_MISSING, "project node references missing loaded unit");
        for (size_t p = 0; p < loaded->unit.params_len; p++) {
            params[out_index]      = loaded->unit.params[p];
            params[out_index].name = arena_join_dot(arena, project_node->id, loaded->unit.params[p].name, err);
            if (!params[out_index].name || err->status != UC_OK)
                return err->status;
            const apg_project_v2_param_override_t *override = NULL;
            for (size_t k = 0; k < project_node->params_len; k++) {
                if (project_node->params[k].key &&
                    strcmp(project_node->params[k].key, loaded->unit.params[p].name) == 0) {
                    override = &project_node->params[k];
                    break;
                }
            }
            if (override)
                params[out_index].default_value = override->value.text;
            out_index++;
        }
    }

    unit->params     = params;
    unit->params_len = params_len;
    return UC_OK;
}

static uc_status
build_expanded_ports(const apg_project_v2_resolved_t *project, uc_arena *arena, apg_unit_v2_t *unit, uc_error *err) {
    apg_unit_v2_port_t *inputs  = uc_arena_alloc(arena, sizeof(*inputs), sizeof(void *));
    apg_unit_v2_port_t *outputs = uc_arena_alloc(arena, sizeof(*outputs), sizeof(void *));
    const char        **out_sig = uc_arena_alloc(arena, sizeof(*out_sig), sizeof(void *));
    if (!inputs || !outputs || !out_sig)
        return set_error(err, UC_E_OOM, "arena OOM");

    memset(inputs, 0, sizeof(*inputs));
    memset(outputs, 0, sizeof(*outputs));

    const char *output_signal = project_output_signal(project);
    if (!output_signal)
        return set_error(err, UC_E_MISSING, "project output route is missing");

    inputs[0].name     = "input";
    inputs[0].type     = "audio";
    inputs[0].channels = "1";

    outputs[0].name        = "output";
    outputs[0].type        = "audio";
    outputs[0].channels    = "1";
    outputs[0].signals     = out_sig;
    outputs[0].signals_len = 1u;
    out_sig[0]             = output_signal;

    unit->input_ports      = inputs;
    unit->input_ports_len  = 1u;
    unit->output_ports     = outputs;
    unit->output_ports_len = 1u;
    return UC_OK;
}

static uc_status
build_expanded_signals(const apg_project_v2_resolved_t *project, uc_arena *arena, apg_unit_v2_t *unit, uc_error *err) {
    size_t max_signals = 2u;
    for (size_t i = 0; i < project->project.nodes_len; i++) {
        const apg_project_v2_loaded_unit_t *loaded = find_loaded_unit(project, project->project.nodes[i].unit);
        max_signals += loaded ? loaded->unit.signals_len : 0u;
    }

    const char **signals = uc_arena_alloc(arena, max_signals * sizeof(*signals), sizeof(void *));
    if (!signals && max_signals > 0u)
        return set_error(err, UC_E_OOM, "arena OOM");
    size_t signals_len = 0;
    add_signal(&signals, &signals_len, "input");
    add_signal(&signals, &signals_len, project_output_signal(project));

    for (size_t i = 0; i < project->project.nodes_len; i++) {
        const apg_project_v2_node_t        *project_node = &project->project.nodes[i];
        const apg_project_v2_loaded_unit_t *loaded       = find_loaded_unit(project, project_node->unit);
        if (!loaded)
            return set_error(err, UC_E_MISSING, "project node references missing loaded unit");
        for (size_t s = 0; s < loaded->unit.signals_len; s++) {
            const char *mapped =
                remap_signal(project, project_node, &loaded->unit, loaded->unit.signals[s], arena, err);
            if (!mapped || err->status != UC_OK)
                return err->status;
            add_signal(&signals, &signals_len, mapped);
        }
    }

    unit->signals     = signals;
    unit->signals_len = signals_len;
    return UC_OK;
}

static uc_status
build_expanded_nodes(const apg_project_v2_resolved_t *project, uc_arena *arena, apg_unit_v2_t *unit, uc_error *err) {
    size_t nodes_len = 0;
    for (size_t i = 0; i < project->project.nodes_len; i++) {
        const apg_project_v2_loaded_unit_t *loaded = find_loaded_unit(project, project->project.nodes[i].unit);
        nodes_len += loaded ? loaded->unit.nodes_len : 0u;
    }

    apg_unit_v2_node_t *nodes = uc_arena_alloc(arena, nodes_len * sizeof(*nodes), sizeof(void *));
    if (!nodes && nodes_len > 0u)
        return set_error(err, UC_E_OOM, "arena OOM");

    size_t out_index = 0;
    for (size_t i = 0; i < project->project.nodes_len; i++) {
        const apg_project_v2_node_t        *project_node = &project->project.nodes[i];
        const apg_project_v2_loaded_unit_t *loaded       = find_loaded_unit(project, project_node->unit);
        if (!loaded)
            return set_error(err, UC_E_MISSING, "project node references missing loaded unit");
        for (size_t n = 0; n < loaded->unit.nodes_len; n++) {
            const apg_unit_v2_node_t *src = &loaded->unit.nodes[n];
            nodes[out_index]              = *src;
            nodes[out_index].id           = arena_join_dot(arena, project_node->id, src->id, err);
            if (!nodes[out_index].id || err->status != UC_OK)
                return err->status;
            uc_status status = clone_bindings(
                project, project_node, &loaded->unit, src->in, src->in_len, true, arena, &nodes[out_index].in,
                &nodes[out_index].in_len, err
            );
            if (status != UC_OK)
                return status;
            status = clone_bindings(
                project, project_node, &loaded->unit, src->out, src->out_len, true, arena, &nodes[out_index].out,
                &nodes[out_index].out_len, err
            );
            if (status != UC_OK)
                return status;
            status = clone_bindings(
                project, project_node, &loaded->unit, src->config, src->config_len, false, arena,
                &nodes[out_index].config, &nodes[out_index].config_len, err
            );
            if (status != UC_OK)
                return status;
            out_index++;
        }
    }

    unit->nodes     = nodes;
    unit->nodes_len = nodes_len;
    return UC_OK;
}

static uc_status
build_expanded_unit(const apg_project_v2_resolved_t *project, uc_arena *arena, apg_unit_v2_t *unit, uc_error *err) {
    memset(unit, 0, sizeof(*unit));
    unit->name    = project->project.name;
    unit->version = project->project.version;

    uc_status status = build_expanded_params(project, arena, unit, err);
    if (status != UC_OK)
        return status;
    status = build_expanded_ports(project, arena, unit, err);
    if (status != UC_OK)
        return status;
    status = build_expanded_signals(project, arena, unit, err);
    if (status != UC_OK)
        return status;
    return build_expanded_nodes(project, arena, unit, err);
}

uc_status apg_project_v2_compile(
    const apg_project_v2_resolved_t *project, uc_arena *arena, apg_project_v2_compiled_t *out, uc_error *err
) {
    if (!project || !arena || !out || !err)
        return UC_E_TYPE;
    memset(out, 0, sizeof(*out));
    err->status = UC_OK;

    uc_status status = apg_project_v2_validate_resolved(project, err);
    if (status != UC_OK)
        return status;
    status = build_expanded_unit(project, arena, &out->expanded_unit, err);
    if (status != UC_OK)
        return status;
    return apg_v2_compile_unit(&out->expanded_unit, arena, &out->plan, err);
}
