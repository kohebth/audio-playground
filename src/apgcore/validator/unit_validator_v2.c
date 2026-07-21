#include <apgcore/validator/unit_validator_v2.h>

#include <apgcore/metadata/atom_catalog.h>
#include <yaml/node.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uc_status set_error(uc_error *err, uc_status status, const char *msg) {
    uc_loc loc = {0, 0};
    uc_error_set(err, status, loc, "%s", msg);
    return status;
}

static const char *node_scalar(const uc_node *node) { return node && node->kind == UC_NODE_SCALAR ? node->text : NULL; }

static const char *value_text(const uc_node *node) {
    if (!node)
        return NULL;
    return node->kind == UC_NODE_SCALAR || node->kind == UC_NODE_VARREF ? node->text : NULL;
}

static bool scalar_eq(const uc_node *node, const char *expected) {
    return node && node->kind == UC_NODE_SCALAR && strcmp(node->text, expected) == 0;
}

static const char *required_scalar(const uc_node *map, const char *key, uc_error *err) {
    const uc_node *node = uc_node_find(map, key);
    if (!node || node->kind != UC_NODE_SCALAR) {
        char msg[128];
        snprintf(msg, sizeof(msg), "missing scalar field '%s'", key);
        set_error(err, UC_E_MISSING, msg);
        return NULL;
    }
    return node->text;
}

static const uc_node *optional_map_path(const uc_node *map, const char *key, const char *path, uc_error *err) {
    const uc_node *node = uc_node_find(map, key);
    if (!node)
        return NULL;
    if (node->kind != UC_NODE_MAP) {
        char msg[160];
        snprintf(msg, sizeof(msg), "%s must be a map", path ? path : key);
        set_error(err, UC_E_TYPE, msg);
        return NULL;
    }
    return node;
}

static const char *optional_scalar_path(const uc_node *map, const char *key, const char *path, uc_error *err) {
    const uc_node *node = uc_node_find(map, key);
    if (!node)
        return NULL;
    if (node->kind != UC_NODE_SCALAR) {
        char msg[160];
        snprintf(msg, sizeof(msg), "%s must be a scalar", path ? path : key);
        set_error(err, UC_E_TYPE, msg);
        return NULL;
    }
    return node->text;
}

static bool map_has_key(const uc_node *map, const char *key) { return uc_node_find(map, key) != NULL; }

static bool signal_list_contains(const char **signals, size_t signals_len, const char *value) {
    if (!signals || !value)
        return false;
    for (size_t i = 0; i < signals_len; i++) {
        if (signals[i] && strcmp(signals[i], value) == 0)
            return true;
    }
    return false;
}

static bool param_exists(const uc_node *params, const char *name) {
    return params && params->kind == UC_NODE_MAP && map_has_key(params, name);
}

static bool param_type_is_valid(const char *type) {
    return type && (strcmp(type, "float") == 0 || strcmp(type, "int") == 0 || strcmp(type, "bool") == 0);
}

static bool param_type_is_numeric(const char *type) {
    return type && (strcmp(type, "float") == 0 || strcmp(type, "int") == 0);
}

static bool port_type_is_valid(const char *type) {
    return type && (strcmp(type, "audio") == 0 || strcmp(type, "control") == 0);
}

static bool port_type_is_audio(const char *type) { return type && strcmp(type, "audio") == 0; }

static bool port_type_is_control(const char *type) { return type && strcmp(type, "control") == 0; }

static apg_v2_value_t to_value(const uc_node *node) {
    apg_v2_value_t value = {APG_V2_VALUE_LITERAL, ""};
    if (!node)
        return value;
    if (node->kind == UC_NODE_VARREF) {
        value.kind = APG_V2_VALUE_VARREF;
        value.text = node->text;
    } else if (node->kind == UC_NODE_SCALAR) {
        value.kind = APG_V2_VALUE_LITERAL;
        value.text = node->text;
    }
    return value;
}

static const char *param_ref_name(const char *text, char *buf, size_t buf_size) {
    if (!text || !buf || buf_size == 0)
        return NULL;

    const char *start = NULL;
    const char *end   = NULL;
    if (strncmp(text, "${params.", 9) == 0) {
        start = text + 9;
        end   = strchr(start, '}');
    } else if (strncmp(text, "params.", 7) == 0) {
        start = text + 7;
        end   = start + strlen(start);
    } else {
        return NULL;
    }

    if (!end || end <= start)
        return NULL;

    size_t len = (size_t)(end - start);
    if (len >= buf_size)
        len = buf_size - 1;
    memcpy(buf, start, len);
    buf[len] = '\0';
    return buf;
}

static uc_status validate_param_refs(const uc_node *node, const uc_node *params, uc_error *err) {
    if (!node)
        return UC_OK;

    if (node->kind == UC_NODE_VARREF || node->kind == UC_NODE_SCALAR) {
        char        name[64];
        const char *ref = param_ref_name(node->text, name, sizeof(name));
        if (ref && !param_exists(params, ref)) {
            char msg[128];
            snprintf(msg, sizeof(msg), "unknown parameter reference '%s'", node->text);
            return set_error(err, UC_E_MISSING, msg);
        }
        return UC_OK;
    }

    if (node->kind == UC_NODE_MAP) {
        for (size_t i = 0; i < node->map_len; i++) {
            uc_status status = validate_param_refs(node->map[i].value, params, err);
            if (status != UC_OK)
                return status;
        }
    } else if (node->kind == UC_NODE_SEQ) {
        for (size_t i = 0; i < node->seq_len; i++) {
            uc_status status = validate_param_refs(node->seq[i], params, err);
            if (status != UC_OK)
                return status;
        }
    }

    return UC_OK;
}

static bool param_ui_control_is_valid(const char *control) {
    return !control || strcmp(control, "knob") == 0 || strcmp(control, "slider") == 0 ||
           strcmp(control, "toggle") == 0 || strcmp(control, "number") == 0 || strcmp(control, "select") == 0;
}

static bool param_ui_scale_is_valid(const char *scale) {
    return !scale || strcmp(scale, "linear") == 0 || strcmp(scale, "log") == 0 || strcmp(scale, "exp") == 0;
}

static bool nonnegative_uint_text_is_valid(const char *text) {
    if (!text || text[0] == '\0')
        return false;
    char         *end   = NULL;
    unsigned long value = strtoul(text, &end, 10);
    (void)value;
    return end && *end == '\0';
}

static uc_status validate_and_fill_meta(const uc_node *root, apg_unit_v2_t *out, uc_error *err) {
    const uc_node *meta = optional_map_path(root, "meta", "meta", err);
    if (!meta)
        return err->status == UC_OK ? UC_OK : err->status;

    out->meta.title       = optional_scalar_path(meta, "title", "meta.title", err);
    out->meta.category    = optional_scalar_path(meta, "category", "meta.category", err);
    out->meta.description = optional_scalar_path(meta, "description", "meta.description", err);
    return err->status == UC_OK ? UC_OK : err->status;
}

static uc_status validate_unit_ui(const uc_node *root, uc_error *err) {
    const uc_node *ui = optional_map_path(root, "ui", "ui", err);
    (void)ui;
    return err->status == UC_OK ? UC_OK : err->status;
}

static uc_status fill_param_ui(const uc_node *param, const char *param_name, apg_unit_v2_param_t *out, uc_error *err) {
    char path[128];
    snprintf(path, sizeof(path), "params.%s.ui", param_name ? param_name : "");
    const uc_node *ui = optional_map_path(param, "ui", path, err);
    if (!ui)
        return err->status == UC_OK ? UC_OK : err->status;

    snprintf(path, sizeof(path), "params.%s.ui.label", param_name ? param_name : "");
    out->ui_label = optional_scalar_path(ui, "label", path, err);
    if (err->status != UC_OK)
        return err->status;

    snprintf(path, sizeof(path), "params.%s.ui.control", param_name ? param_name : "");
    out->ui_control = optional_scalar_path(ui, "control", path, err);
    if (err->status != UC_OK)
        return err->status;
    if (!param_ui_control_is_valid(out->ui_control)) {
        char msg[192];
        snprintf(msg, sizeof(msg), "%s must be one of knob, slider, toggle, number, or select", path);
        return set_error(err, UC_E_TYPE, msg);
    }

    snprintf(path, sizeof(path), "params.%s.ui.unit", param_name ? param_name : "");
    out->ui_unit = optional_scalar_path(ui, "unit", path, err);
    if (err->status != UC_OK)
        return err->status;

    snprintf(path, sizeof(path), "params.%s.ui.scale", param_name ? param_name : "");
    out->ui_scale = optional_scalar_path(ui, "scale", path, err);
    if (err->status != UC_OK)
        return err->status;
    if (!param_ui_scale_is_valid(out->ui_scale)) {
        char msg[192];
        snprintf(msg, sizeof(msg), "%s must be one of linear, log, or exp", path);
        return set_error(err, UC_E_TYPE, msg);
    }

    snprintf(path, sizeof(path), "params.%s.ui.display_precision", param_name ? param_name : "");
    out->ui_display_precision = optional_scalar_path(ui, "display_precision", path, err);
    if (err->status != UC_OK)
        return err->status;
    if (out->ui_display_precision && !nonnegative_uint_text_is_valid(out->ui_display_precision)) {
        char msg[192];
        snprintf(msg, sizeof(msg), "%s must be a non-negative integer", path);
        return set_error(err, UC_E_RANGE, msg);
    }

    return UC_OK;
}

static uc_status fill_bindings(
    const uc_node *map, uc_arena *arena, apg_unit_v2_binding_t **out_bindings, size_t *out_len, uc_error *err
) {
    *out_bindings = NULL;
    *out_len      = 0;
    if (!map)
        return UC_OK;
    if (map->kind != UC_NODE_MAP)
        return set_error(err, UC_E_TYPE, "node binding section must be a map");

    apg_unit_v2_binding_t *bindings = uc_arena_alloc(arena, map->map_len * sizeof(*bindings), sizeof(void *));
    if (!bindings && map->map_len > 0)
        return set_error(err, UC_E_OOM, "arena OOM");

    for (size_t i = 0; i < map->map_len; i++) {
        for (size_t j = 0; j < i; j++) {
            if (strcmp(map->map[j].key, map->map[i].key) == 0) {
                char msg[128];
                snprintf(msg, sizeof(msg), "duplicate node binding key '%s'", map->map[i].key ? map->map[i].key : "");
                return set_error(err, UC_E_RANGE, msg);
            }
        }
        bindings[i].key   = map->map[i].key;
        bindings[i].value = to_value(map->map[i].value);
        bindings[i].node  = map->map[i].value;
    }

    *out_bindings = bindings;
    *out_len      = map->map_len;
    return UC_OK;
}

static uc_status append_signal_name(
    const char *signal, uc_arena *arena, const char ***signals, size_t *signals_len, size_t *signals_cap, uc_error *err
) {
    if (!signal || signal[0] == '\0')
        return set_error(err, UC_E_TYPE, "graph signal name must be non-empty");
    if (signal_list_contains(*signals, *signals_len, signal))
        return UC_OK;
    if (*signals_len >= *signals_cap) {
        size_t       next_cap = *signals_cap == 0u ? 8u : *signals_cap * 2u;
        const char **next     = uc_arena_alloc(arena, next_cap * sizeof(*next), sizeof(void *));
        if (!next)
            return set_error(err, UC_E_OOM, "arena OOM");
        for (size_t i = 0; i < *signals_len; i++)
            next[i] = (*signals)[i];
        *signals     = next;
        *signals_cap = next_cap;
    }
    (*signals)[(*signals_len)++] = signal;
    return UC_OK;
}

static uc_status validate_and_fill_params(const uc_node *params, uc_arena *arena, apg_unit_v2_t *out, uc_error *err) {
    if (!params || params->kind != UC_NODE_MAP)
        return set_error(err, UC_E_MISSING, "missing map field 'params'");

    apg_unit_v2_param_t *items = uc_arena_alloc(arena, params->map_len * sizeof(*items), sizeof(void *));
    if (!items && params->map_len > 0)
        return set_error(err, UC_E_OOM, "arena OOM");

    for (size_t i = 0; i < params->map_len; i++) {
        for (size_t j = 0; j < i; j++) {
            if (strcmp(params->map[j].key, params->map[i].key) == 0) {
                char msg[128];
                snprintf(msg, sizeof(msg), "duplicate param name '%s'", params->map[i].key ? params->map[i].key : "");
                return set_error(err, UC_E_RANGE, msg);
            }
        }

        const uc_node *param = params->map[i].value;
        if (!param || param->kind != UC_NODE_MAP)
            return set_error(err, UC_E_TYPE, "param entry must be a map");

        const char *type = required_scalar(param, "type", err);
        if (!type)
            return err->status;
        if (!param_type_is_valid(type)) {
            char msg[160];
            snprintf(
                msg, sizeof(msg), "param '%s' type must be 'float', 'int', or 'bool'",
                params->map[i].key ? params->map[i].key : ""
            );
            return set_error(err, UC_E_TYPE, msg);
        }
        const char *default_value = value_text(uc_node_find(param, "default"));
        const char *min_value     = value_text(uc_node_find(param, "min"));
        const char *max_value     = value_text(uc_node_find(param, "max"));
        if (!default_value) {
            char msg[128];
            snprintf(msg, sizeof(msg), "param '%s' missing 'default'", params->map[i].key ? params->map[i].key : "");
            return set_error(err, UC_E_MISSING, msg);
        }
        if (param_type_is_numeric(type)) {
            if (!min_value) {
                char msg[128];
                snprintf(
                    msg, sizeof(msg), "numeric param '%s' missing 'min'", params->map[i].key ? params->map[i].key : ""
                );
                return set_error(err, UC_E_MISSING, msg);
            }
            if (!max_value) {
                char msg[128];
                snprintf(
                    msg, sizeof(msg), "numeric param '%s' missing 'max'", params->map[i].key ? params->map[i].key : ""
                );
                return set_error(err, UC_E_MISSING, msg);
            }
        }

        items[i].name          = params->map[i].key;
        items[i].type          = type;
        items[i].default_value = default_value;
        items[i].min_value     = min_value;
        items[i].max_value     = max_value;
        items[i].smoothing_ms  = value_text(uc_node_find(param, "smoothing_ms"));
        uc_status status       = fill_param_ui(param, params->map[i].key, &items[i], err);
        if (status != UC_OK)
            return status;
    }

    out->params     = items;
    out->params_len = params->map_len;
    return UC_OK;
}

static bool parse_channel_count(const char *text, size_t *out_count) {
    if (!text || !out_count || text[0] == '\0')
        return false;

    char         *end   = NULL;
    unsigned long value = strtoul(text, &end, 10);
    if (!end || *end != '\0' || value == 0ul)
        return false;

    *out_count = (size_t)value;
    return true;
}

static uc_status fill_control_port_target(
    const uc_node *port,
    const uc_node *params,
    const char    *name,
    const char    *target_param,
    const char   **out_kind,
    const char   **out_name,
    uc_error      *err
) {
    *out_kind = "param";
    *out_name = name;

    const uc_node *target = uc_node_find(port, "target");
    if (target_param && target) {
        char msg[160];
        snprintf(msg, sizeof(msg), "control port '%s' cannot declare both target_param and target", name ? name : "");
        return set_error(err, UC_E_TYPE, msg);
    }

    if (target_param) {
        if (!param_exists(params, target_param)) {
            char msg[160];
            snprintf(msg, sizeof(msg), "control port '%s' target_param references unknown param", name ? name : "");
            return set_error(err, UC_E_MISSING, msg);
        }
        *out_name = target_param;
        return UC_OK;
    }

    if (!target)
        return UC_OK;
    if (target->kind != UC_NODE_MAP) {
        char msg[160];
        snprintf(msg, sizeof(msg), "control port '%s' target must be a map", name ? name : "");
        return set_error(err, UC_E_TYPE, msg);
    }

    const char *kind = required_scalar(target, "kind", err);
    if (!kind)
        return err->status;
    if (strcmp(kind, "param") != 0) {
        char msg[192];
        snprintf(
            msg, sizeof(msg), "control port '%s' target kind '%s' is unsupported", name ? name : "", kind ? kind : ""
        );
        return set_error(err, UC_E_TYPE, msg);
    }

    const char *target_name = required_scalar(target, "name", err);
    if (!target_name)
        return err->status;
    if (!param_exists(params, target_name)) {
        char msg[192];
        snprintf(msg, sizeof(msg), "control port '%s' target param '%s' is unknown", name ? name : "", target_name);
        return set_error(err, UC_E_MISSING, msg);
    }

    *out_kind = kind;
    *out_name = target_name;
    return UC_OK;
}

static uc_status fill_audio_port_signals(
    const uc_node *port,
    const char   **graph_signals,
    size_t         graph_signals_len,
    const char    *name,
    size_t         channel_count,
    uc_arena      *arena,
    const char  ***out_signals,
    size_t        *out_len,
    uc_error      *err
) {
    *out_signals = NULL;
    *out_len     = 0;

    const uc_node *port_signals = uc_node_find(port, "signals");
    if (!port_signals) {
        if (channel_count == 1u) {
            if (!signal_list_contains(graph_signals, graph_signals_len, name)) {
                char msg[160];
                snprintf(msg, sizeof(msg), "public audio port '%s' is missing matching graph signal", name ? name : "");
                return set_error(err, UC_E_MISSING, msg);
            }
            return UC_OK;
        }

        char msg[160];
        snprintf(msg, sizeof(msg), "multi-channel audio port '%s' missing 'signals'", name ? name : "");
        return set_error(err, UC_E_MISSING, msg);
    }

    if (port_signals->kind != UC_NODE_SEQ)
        return set_error(err, UC_E_TYPE, "audio port signals must be a sequence");
    if (port_signals->seq_len != channel_count) {
        char msg[160];
        snprintf(msg, sizeof(msg), "audio port '%s' signals count must match channels", name ? name : "");
        return set_error(err, UC_E_RANGE, msg);
    }

    const char **items = uc_arena_alloc(arena, port_signals->seq_len * sizeof(*items), sizeof(void *));
    if (!items && port_signals->seq_len > 0)
        return set_error(err, UC_E_OOM, "arena OOM");

    for (size_t i = 0; i < port_signals->seq_len; i++) {
        const char *signal = node_scalar(port_signals->seq[i]);
        if (!signal)
            return set_error(err, UC_E_TYPE, "audio port signal entry must be a scalar");
        if (!signal_list_contains(graph_signals, graph_signals_len, signal)) {
            char msg[160];
            snprintf(msg, sizeof(msg), "audio port '%s' references unknown signal '%s'", name ? name : "", signal);
            return set_error(err, UC_E_MISSING, msg);
        }
        items[i] = signal;
    }

    *out_signals = items;
    *out_len     = port_signals->seq_len;
    return UC_OK;
}

static uc_status fill_port_group(
    const uc_node       *seq,
    const char         **signals,
    size_t               signals_len,
    const uc_node       *params,
    uc_arena            *arena,
    apg_unit_v2_port_t **out_ports,
    size_t              *out_len,
    uc_error            *err
) {
    if (!seq || seq->kind != UC_NODE_SEQ)
        return set_error(err, UC_E_MISSING, "missing port sequence");

    apg_unit_v2_port_t *ports = uc_arena_alloc(arena, seq->seq_len * sizeof(*ports), sizeof(void *));
    if (!ports && seq->seq_len > 0)
        return set_error(err, UC_E_OOM, "arena OOM");

    for (size_t i = 0; i < seq->seq_len; i++) {
        const uc_node *port = seq->seq[i];
        if (!port || port->kind != UC_NODE_MAP)
            return set_error(err, UC_E_TYPE, "port entry must be a map");

        const char *name = required_scalar(port, "name", err);
        if (!name)
            return err->status;
        for (size_t j = 0; j < i; j++) {
            if (strcmp(ports[j].name, name) == 0) {
                char msg[128];
                snprintf(msg, sizeof(msg), "duplicate port name '%s'", name ? name : "");
                return set_error(err, UC_E_RANGE, msg);
            }
        }
        const char *type = required_scalar(port, "type", err);
        if (!type)
            return err->status;
        if (!port_type_is_valid(type)) {
            char msg[160];
            snprintf(msg, sizeof(msg), "port '%s' type must be 'audio' or 'control'", name ? name : "");
            return set_error(err, UC_E_TYPE, msg);
        }

        const char  *channels         = value_text(uc_node_find(port, "channels"));
        const char  *target_param     = value_text(uc_node_find(port, "target_param"));
        const char  *target_kind      = NULL;
        const char  *target_name      = NULL;
        const char **port_signals     = NULL;
        size_t       port_signals_len = 0;
        if (port_type_is_audio(type)) {
            if (target_param || uc_node_find(port, "target")) {
                char msg[160];
                snprintf(msg, sizeof(msg), "audio port '%s' cannot declare a control target", name ? name : "");
                return set_error(err, UC_E_TYPE, msg);
            }
            if (!channels) {
                char msg[128];
                snprintf(msg, sizeof(msg), "audio port '%s' missing 'channels'", name ? name : "");
                return set_error(err, UC_E_MISSING, msg);
            }
            size_t channel_count = 0;
            if (!parse_channel_count(channels, &channel_count)) {
                char msg[128];
                snprintf(msg, sizeof(msg), "audio port '%s' has invalid channels", name ? name : "");
                return set_error(err, UC_E_RANGE, msg);
            }
            uc_status status = fill_audio_port_signals(
                port, signals, signals_len, name, channel_count, arena, &port_signals, &port_signals_len, err
            );
            if (status != UC_OK)
                return status;
        } else if (port_type_is_control(type)) {
            uc_status status =
                fill_control_port_target(port, params, name, target_param, &target_kind, &target_name, err);
            if (status != UC_OK)
                return status;
        }

        ports[i].name         = name;
        ports[i].type         = type;
        ports[i].channels     = channels;
        ports[i].target_param = target_param;
        ports[i].target_kind  = target_kind;
        ports[i].target_name  = target_name;
        ports[i].signals      = port_signals;
        ports[i].signals_len  = port_signals_len;
    }

    *out_ports = ports;
    *out_len   = seq->seq_len;
    return UC_OK;
}

static uc_status validate_and_fill_ports(
    const uc_node *ports,
    const char   **signals,
    size_t         signals_len,
    const uc_node *params,
    uc_arena      *arena,
    apg_unit_v2_t *out,
    uc_error      *err
) {
    if (!ports || ports->kind != UC_NODE_MAP)
        return set_error(err, UC_E_MISSING, "missing map field 'ports'");

    const uc_node *inputs  = uc_node_find(ports, "inputs");
    const uc_node *outputs = uc_node_find(ports, "outputs");
    uc_status      status =
        fill_port_group(inputs, signals, signals_len, params, arena, &out->input_ports, &out->input_ports_len, err);
    if (status != UC_OK)
        return status;
    return fill_port_group(
        outputs, signals, signals_len, params, arena, &out->output_ports, &out->output_ports_len, err
    );
}

typedef enum {
    APG_UNIT_ENDPOINT_SIGNAL,
    APG_UNIT_ENDPOINT_NODE_FIELD,
} apg_unit_endpoint_kind_t;

typedef struct {
    apg_unit_endpoint_kind_t kind;
    const char              *node_id;
    const char              *field;
    const char              *signal;
} apg_unit_route_endpoint_t;

typedef struct {
    apg_unit_route_endpoint_t from;
    apg_unit_route_endpoint_t to;
    const uc_node            *node;
} apg_unit_route_t;

typedef struct {
    const char    *id;
    const char    *atom;
    const uc_node *node;
    bool           pseudo_input;
    bool           pseudo_output;
    size_t         output_index;
} apg_unit_node_source_t;

static const char *arena_strndup(uc_arena *arena, const char *src, size_t len, uc_error *err) {
    char *copy = uc_arena_alloc(arena, len + 1u, 1u);
    if (!copy) {
        set_error(err, UC_E_OOM, "arena OOM");
        return NULL;
    }
    memcpy(copy, src, len);
    copy[len] = '\0';
    return copy;
}

static const char *arena_printf_signal(uc_arena *arena, const char *a, const char *b, uc_error *err) {
    size_t a_len = a ? strlen(a) : 0u;
    size_t b_len = b ? strlen(b) : 0u;
    char  *copy  = uc_arena_alloc(arena, a_len + 1u + b_len + 1u, 1u);
    if (!copy) {
        set_error(err, UC_E_OOM, "arena OOM");
        return NULL;
    }
    memcpy(copy, a ? a : "", a_len);
    copy[a_len] = '.';
    memcpy(copy + a_len + 1u, b ? b : "", b_len);
    copy[a_len + 1u + b_len] = '\0';
    return copy;
}

static const char *trim_span(const char **start, const char *end) {
    const char *s = *start;
    while (s < end && (*s == ' ' || *s == '\t'))
        s++;
    while (end > s && (end[-1] == ' ' || end[-1] == '\t'))
        end--;
    *start = s;
    return end;
}

static uc_status parse_route_endpoint(
    const char *start, const char *end, uc_arena *arena, apg_unit_route_endpoint_t *out, uc_error *err
) {
    end = trim_span(&start, end);
    if (start >= end)
        return set_error(err, UC_E_TYPE, "graph route endpoint must be non-empty");

    const char *dot = NULL;
    for (const char *p = start; p < end; p++) {
        if (*p == '.') {
            dot = p;
            break;
        }
    }

    memset(out, 0, sizeof(*out));
    if (!dot) {
        out->kind   = APG_UNIT_ENDPOINT_SIGNAL;
        out->signal = arena_strndup(arena, start, (size_t)(end - start), err);
        return out->signal ? UC_OK : err->status;
    }
    if (dot == start || dot + 1 >= end)
        return set_error(err, UC_E_TYPE, "graph route node endpoint must be node.field");

    out->kind    = APG_UNIT_ENDPOINT_NODE_FIELD;
    out->node_id = arena_strndup(arena, start, (size_t)(dot - start), err);
    if (!out->node_id)
        return err->status;
    out->field = arena_strndup(arena, dot + 1, (size_t)(end - dot - 1), err);
    return out->field ? UC_OK : err->status;
}

static uc_status parse_route_string(const uc_node *node, uc_arena *arena, apg_unit_route_t *out, uc_error *err) {
    const char *text = node_scalar(node);
    if (!text)
        return set_error(err, UC_E_TYPE, "graph route must be a scalar string");

    const char *arrow = strstr(text, "->");
    if (!arrow)
        return set_error(err, UC_E_TYPE, "graph route must use 'from -> to'");
    if (strstr(arrow + 2, "->"))
        return set_error(err, UC_E_TYPE, "graph route must contain exactly one '->'");

    memset(out, 0, sizeof(*out));
    out->node        = node;
    uc_status status = parse_route_endpoint(text, arrow, arena, &out->from, err);
    if (status != UC_OK)
        return status;
    return parse_route_endpoint(arrow + 2, text + strlen(text), arena, &out->to, err);
}

static uc_status
fill_routes(const uc_node *routes, uc_arena *arena, apg_unit_route_t **out_routes, size_t *out_len, uc_error *err) {
    *out_routes = NULL;
    *out_len    = 0;
    if (!routes)
        return UC_OK;
    if (routes->kind != UC_NODE_SEQ)
        return set_error(err, UC_E_TYPE, "graph.routes must be a sequence");

    apg_unit_route_t *items = uc_arena_alloc(arena, routes->seq_len * sizeof(*items), sizeof(void *));
    if (!items && routes->seq_len > 0u)
        return set_error(err, UC_E_OOM, "arena OOM");
    for (size_t i = 0; i < routes->seq_len; i++) {
        uc_status status = parse_route_string(routes->seq[i], arena, &items[i], err);
        if (status != UC_OK)
            return status;
    }
    *out_routes = items;
    *out_len    = routes->seq_len;
    return UC_OK;
}

static const apg_unit_node_source_t *
find_node_source(const apg_unit_node_source_t *nodes, size_t nodes_len, const char *id) {
    for (size_t i = 0; i < nodes_len; i++) {
        if (nodes[i].id && id && strcmp(nodes[i].id, id) == 0)
            return &nodes[i];
    }
    return NULL;
}

static bool atom_contract_has_exactly_one_signal_field(
    const char *atom, apg_atom_contract_section_t section, const char **out_field
) {
    size_t      count = apg_atom_contract_field_count(atom, section);
    const char *field = NULL;
    size_t      seen  = 0u;
    for (size_t i = 0; i < count; i++) {
        apg_atom_contract_field_t item;
        if (!apg_atom_contract_field(atom, section, i, &item))
            continue;
        if (item.type != APG_ATOM_FIELD_SIGNAL)
            continue;
        field = item.name;
        seen++;
    }
    if (seen == 1u && out_field)
        *out_field = field;
    return seen == 1u;
}

static const char *resolve_route_field(
    const apg_unit_node_source_t *node, const char *field, apg_atom_contract_section_t section, uc_error *err
) {
    if (!node || !field)
        return NULL;
    if (strcmp(field, "in") != 0 && strcmp(field, "out") != 0)
        return field;

    const char *resolved = NULL;
    if (atom_contract_has_exactly_one_signal_field(node->atom, section, &resolved))
        return resolved;

    char msg[192];
    snprintf(
        msg, sizeof(msg), "node '%s' route endpoint '%s' is ambiguous for atom '%s'", node->id ? node->id : "", field,
        node->atom ? node->atom : ""
    );
    set_error(err, UC_E_MISSING, msg);
    return NULL;
}

static uc_status append_binding(
    apg_unit_v2_binding_t *bindings,
    size_t                *bindings_len,
    const char            *node_id,
    const char            *section,
    const char            *key,
    apg_v2_value_t         value,
    const uc_node         *value_node,
    uc_error              *err
) {
    for (size_t i = 0; i < *bindings_len; i++) {
        if (bindings[i].key && key && strcmp(bindings[i].key, key) == 0) {
            char msg[192];
            snprintf(
                msg, sizeof(msg), "node '%s' has duplicate %s binding key '%s'", node_id ? node_id : "",
                section ? section : "route", key ? key : ""
            );
            return set_error(err, UC_E_RANGE, msg);
        }
    }
    bindings[*bindings_len].key   = key;
    bindings[*bindings_len].value = value;
    bindings[*bindings_len].node  = value_node;
    (*bindings_len)++;
    return UC_OK;
}

static uc_status copy_bindings_to(
    const uc_node         *map,
    const char            *node_id,
    const char            *section,
    apg_unit_v2_binding_t *bindings,
    size_t                *bindings_len,
    uc_error              *err
) {
    if (!map)
        return UC_OK;
    if (map->kind != UC_NODE_MAP)
        return set_error(err, UC_E_TYPE, "node binding section must be a map");
    for (size_t i = 0; i < map->map_len; i++) {
        uc_status status = append_binding(
            bindings, bindings_len, node_id, section, map->map[i].key, to_value(map->map[i].value), map->map[i].value,
            err
        );
        if (status != UC_OK)
            return status;
    }
    return UC_OK;
}

static bool node_is_pseudo_io_atom(const char *atom, bool *is_input, bool *is_output) {
    *is_input  = atom && strcmp(atom, "input_signal") == 0;
    *is_output = atom && strcmp(atom, "output_signal") == 0;
    return *is_input || *is_output;
}

static uc_status append_audio_port_signal_names(
    const uc_node *ports,
    uc_arena      *arena,
    const char  ***signals,
    size_t        *signals_len,
    size_t        *signals_cap,
    uc_error      *err
) {
    if (!ports || ports->kind != UC_NODE_MAP)
        return UC_OK;
    const char *groups[] = {"inputs", "outputs"};
    for (size_t g = 0; g < 2u; g++) {
        const uc_node *seq = uc_node_find(ports, groups[g]);
        if (!seq || seq->kind != UC_NODE_SEQ)
            continue;
        for (size_t i = 0; i < seq->seq_len; i++) {
            const uc_node *port = seq->seq[i];
            if (!port || port->kind != UC_NODE_MAP)
                continue;
            const char *type = node_scalar(uc_node_find(port, "type"));
            if (!port_type_is_audio(type))
                continue;
            const char    *name         = node_scalar(uc_node_find(port, "name"));
            const uc_node *port_signals = uc_node_find(port, "signals");
            if (port_signals && port_signals->kind == UC_NODE_SEQ) {
                for (size_t s = 0; s < port_signals->seq_len; s++) {
                    const char *signal = node_scalar(port_signals->seq[s]);
                    uc_status   status = append_signal_name(signal, arena, signals, signals_len, signals_cap, err);
                    if (status != UC_OK)
                        return status;
                }
            } else if (name) {
                uc_status status = append_signal_name(name, arena, signals, signals_len, signals_cap, err);
                if (status != UC_OK)
                    return status;
            }
        }
    }
    return UC_OK;
}

static const char *route_signal_name(
    const apg_unit_route_t       *route,
    const apg_unit_node_source_t *from_node,
    const apg_unit_node_source_t *to_node,
    uc_arena                     *arena,
    uc_error                     *err
);

static uc_status fill_signal_union(
    const uc_node                *signals_node,
    const uc_node                *ports,
    const apg_unit_route_t       *routes,
    size_t                        routes_len,
    const apg_unit_node_source_t *sources,
    size_t                        sources_len,
    uc_arena                     *arena,
    apg_unit_v2_t                *out,
    uc_error                     *err
) {
    const char **signals     = NULL;
    size_t       signals_len = 0u;
    size_t       signals_cap = 0u;

    if (signals_node) {
        if (signals_node->kind != UC_NODE_SEQ)
            return set_error(err, UC_E_MISSING, "missing graph.signals");
        for (size_t i = 0; i < signals_node->seq_len; i++) {
            const char *signal = node_scalar(signals_node->seq[i]);
            if (!signal)
                return set_error(err, UC_E_TYPE, "graph signal must be a scalar");
            if (signal_list_contains(signals, signals_len, signal)) {
                char msg[128];
                snprintf(msg, sizeof(msg), "duplicate graph signal name '%s'", signal ? signal : "");
                return set_error(err, UC_E_RANGE, msg);
            }
            uc_status status = append_signal_name(signal, arena, &signals, &signals_len, &signals_cap, err);
            if (status != UC_OK)
                return status;
        }
    } else if (routes_len == 0u) {
        return set_error(err, UC_E_MISSING, "missing graph.signals");
    }

    uc_status status = UC_OK;
    if (!signals_node) {
        status = append_audio_port_signal_names(ports, arena, &signals, &signals_len, &signals_cap, err);
        if (status != UC_OK)
            return status;
    }

    for (size_t i = 0; i < routes_len; i++) {
        const apg_unit_node_source_t *from_node = routes[i].from.kind == APG_UNIT_ENDPOINT_NODE_FIELD
                                                      ? find_node_source(sources, sources_len, routes[i].from.node_id)
                                                      : NULL;
        const apg_unit_node_source_t *to_node   = routes[i].to.kind == APG_UNIT_ENDPOINT_NODE_FIELD
                                                      ? find_node_source(sources, sources_len, routes[i].to.node_id)
                                                      : NULL;
        if (routes[i].from.kind == APG_UNIT_ENDPOINT_NODE_FIELD && !from_node) {
            char msg[160];
            snprintf(msg, sizeof(msg), "graph route references unknown source node '%s'", routes[i].from.node_id);
            return set_error(err, UC_E_MISSING, msg);
        }
        if (routes[i].to.kind == APG_UNIT_ENDPOINT_NODE_FIELD && !to_node) {
            char msg[160];
            snprintf(msg, sizeof(msg), "graph route references unknown target node '%s'", routes[i].to.node_id);
            return set_error(err, UC_E_MISSING, msg);
        }

        const char *signal = route_signal_name(&routes[i], from_node, to_node, arena, err);
        if (!signal)
            return err->status;
        status = append_signal_name(signal, arena, &signals, &signals_len, &signals_cap, err);
        if (status != UC_OK)
            return status;
    }

    out->signals     = signals;
    out->signals_len = signals_len;
    return UC_OK;
}

static uc_status fill_node_sources(
    const uc_node           *nodes,
    uc_arena                *arena,
    apg_unit_node_source_t **out_sources,
    size_t                  *out_sources_len,
    size_t                  *out_normal_len,
    uc_error                *err
) {
    *out_sources     = NULL;
    *out_sources_len = 0u;
    *out_normal_len  = 0u;
    if (!nodes)
        return set_error(err, UC_E_MISSING, "missing graph.nodes");
    if (nodes->kind != UC_NODE_SEQ && nodes->kind != UC_NODE_MAP)
        return set_error(err, UC_E_MISSING, "missing graph.nodes");

    size_t                  nodes_len = nodes->kind == UC_NODE_SEQ ? nodes->seq_len : nodes->map_len;
    apg_unit_node_source_t *sources   = uc_arena_alloc(arena, nodes_len * sizeof(*sources), sizeof(void *));
    if (!sources && nodes_len > 0u)
        return set_error(err, UC_E_OOM, "arena OOM");

    size_t normal_len = 0u;
    for (size_t i = 0; i < nodes_len; i++) {
        const uc_node *node = nodes->kind == UC_NODE_SEQ ? nodes->seq[i] : nodes->map[i].value;
        if (!node || node->kind != UC_NODE_MAP)
            return set_error(err, UC_E_TYPE, "graph node must be a map");

        const char *id = nodes->kind == UC_NODE_SEQ ? required_scalar(node, "id", err) : nodes->map[i].key;
        if (!id)
            return err->status;
        for (size_t j = 0; j < i; j++) {
            if (strcmp(sources[j].id, id) == 0) {
                char msg[128];
                snprintf(msg, sizeof(msg), "duplicate graph node id '%s'", id ? id : "");
                return set_error(err, UC_E_RANGE, msg);
            }
        }

        const char *atom = required_scalar(node, "atom", err);
        if (!atom)
            return err->status;

        bool pseudo_input  = false;
        bool pseudo_output = false;
        if (!node_is_pseudo_io_atom(atom, &pseudo_input, &pseudo_output) && !apg_atom_known(atom)) {
            char msg[160];
            snprintf(msg, sizeof(msg), "node '%s' references unknown atom '%s'", id ? id : "", atom ? atom : "");
            return set_error(err, UC_E_MISSING, msg);
        }

        sources[i].id            = id;
        sources[i].atom          = atom;
        sources[i].node          = node;
        sources[i].pseudo_input  = pseudo_input;
        sources[i].pseudo_output = pseudo_output;
        sources[i].output_index  = (size_t)-1u;
        if (!pseudo_input && !pseudo_output)
            sources[i].output_index = normal_len++;
    }

    *out_sources     = sources;
    *out_sources_len = nodes_len;
    *out_normal_len  = normal_len;
    return UC_OK;
}

static const char *route_signal_name(
    const apg_unit_route_t       *route,
    const apg_unit_node_source_t *from_node,
    const apg_unit_node_source_t *to_node,
    uc_arena                     *arena,
    uc_error                     *err
) {
    if (route->from.kind == APG_UNIT_ENDPOINT_SIGNAL)
        return route->from.signal;
    if (route->to.kind == APG_UNIT_ENDPOINT_SIGNAL)
        return route->to.signal;
    if (from_node && from_node->pseudo_input)
        return from_node->id;
    if (to_node && to_node->pseudo_output)
        return to_node->id;
    return arena_printf_signal(arena, route->from.node_id, route->from.field, err);
}

static uc_status validate_node_ids_unique(const uc_node *nodes, uc_error *err) {
    for (size_t i = 0; i < nodes->seq_len; i++) {
        const uc_node *a    = nodes->seq[i];
        const char    *a_id = required_scalar(a, "id", err);
        if (!a_id)
            return err->status;
        for (size_t j = i + 1; j < nodes->seq_len; j++) {
            const uc_node *b    = nodes->seq[j];
            const uc_node *b_id = uc_node_find(b, "id");
            if (b_id && b_id->kind == UC_NODE_SCALAR && strcmp(a_id, b_id->text) == 0) {
                char msg[128];
                snprintf(msg, sizeof(msg), "duplicate graph node id '%s'", a_id ? a_id : "");
                return set_error(err, UC_E_RANGE, msg);
            }
        }
    }
    return UC_OK;
}

static uc_status validate_and_fill_nodes(
    const uc_node *nodes, const uc_node *params, uc_arena *arena, apg_unit_v2_t *out, uc_error *err
) {
    if (!nodes || nodes->kind != UC_NODE_SEQ)
        return set_error(err, UC_E_MISSING, "missing graph.nodes");

    uc_status status = validate_node_ids_unique(nodes, err);
    if (status != UC_OK)
        return status;

    apg_unit_v2_node_t *items = uc_arena_alloc(arena, nodes->seq_len * sizeof(*items), sizeof(void *));
    if (!items && nodes->seq_len > 0)
        return set_error(err, UC_E_OOM, "arena OOM");

    for (size_t i = 0; i < nodes->seq_len; i++) {
        const uc_node *node = nodes->seq[i];
        if (!node || node->kind != UC_NODE_MAP)
            return set_error(err, UC_E_TYPE, "graph node must be a map");

        const char *id = required_scalar(node, "id", err);
        if (!id)
            return err->status;
        const char *atom = required_scalar(node, "atom", err);
        if (!atom)
            return err->status;
        if (!apg_atom_known(atom)) {
            char msg[160];
            snprintf(msg, sizeof(msg), "node '%s' references unknown atom '%s'", id ? id : "", atom ? atom : "");
            return set_error(err, UC_E_MISSING, msg);
        }

        status = validate_param_refs(uc_node_find(node, "config"), params, err);
        if (status != UC_OK)
            return status;

        items[i].id   = id;
        items[i].atom = atom;
        status        = fill_bindings(uc_node_find(node, "in"), arena, &items[i].in, &items[i].in_len, err);
        if (status != UC_OK)
            return status;
        status = fill_bindings(uc_node_find(node, "out"), arena, &items[i].out, &items[i].out_len, err);
        if (status != UC_OK)
            return status;
        status = fill_bindings(uc_node_find(node, "config"), arena, &items[i].config, &items[i].config_len, err);
        if (status != UC_OK)
            return status;
    }

    out->nodes     = items;
    out->nodes_len = nodes->seq_len;
    return UC_OK;
}

static uc_status validate_and_fill_routed_nodes(
    const apg_unit_node_source_t *sources,
    size_t                        sources_len,
    size_t                        normal_len,
    const apg_unit_route_t       *routes,
    size_t                        routes_len,
    const uc_node                *params,
    uc_arena                     *arena,
    apg_unit_v2_t                *out,
    uc_error                     *err
) {
    apg_unit_v2_node_t *items = uc_arena_alloc(arena, normal_len * sizeof(*items), sizeof(void *));
    if (!items && normal_len > 0u)
        return set_error(err, UC_E_OOM, "arena OOM");
    if (items && normal_len > 0u)
        memset(items, 0, normal_len * sizeof(*items));

    for (size_t i = 0; i < sources_len; i++) {
        const apg_unit_node_source_t *source = &sources[i];
        if (source->pseudo_input || source->pseudo_output)
            continue;

        apg_unit_v2_node_t *item = &items[source->output_index];
        item->id                 = source->id;
        item->atom               = source->atom;

        const uc_node *in_node     = uc_node_find(source->node, "in");
        const uc_node *out_node    = uc_node_find(source->node, "out");
        size_t         in_cap      = (in_node && in_node->kind == UC_NODE_MAP ? in_node->map_len : 0u) + routes_len;
        size_t         out_cap     = (out_node && out_node->kind == UC_NODE_MAP ? out_node->map_len : 0u) + routes_len;
        size_t         config_cap  = 0u;
        const uc_node *config_node = uc_node_find(source->node, "config");
        const uc_node *params_node = uc_node_find(source->node, "params");
        if (config_node && params_node)
            return set_error(err, UC_E_TYPE, "graph node cannot declare both config and params");
        if (config_node)
            config_cap = config_node->kind == UC_NODE_MAP ? config_node->map_len : 0u;
        if (params_node)
            config_cap = params_node->kind == UC_NODE_MAP ? params_node->map_len : 0u;

        item->in     = uc_arena_alloc(arena, in_cap * sizeof(*item->in), sizeof(void *));
        item->out    = uc_arena_alloc(arena, out_cap * sizeof(*item->out), sizeof(void *));
        item->config = uc_arena_alloc(arena, config_cap * sizeof(*item->config), sizeof(void *));
        if (((!item->in && in_cap > 0u) || (!item->out && out_cap > 0u) || (!item->config && config_cap > 0u)))
            return set_error(err, UC_E_OOM, "arena OOM");

        uc_status status = copy_bindings_to(in_node, item->id, "in", item->in, &item->in_len, err);
        if (status != UC_OK)
            return status;
        status = copy_bindings_to(out_node, item->id, "out", item->out, &item->out_len, err);
        if (status != UC_OK)
            return status;

        const uc_node *effective_config = config_node ? config_node : params_node;
        status                          = validate_param_refs(effective_config, params, err);
        if (status != UC_OK)
            return status;
        status = copy_bindings_to(effective_config, item->id, "config", item->config, &item->config_len, err);
        if (status != UC_OK)
            return status;
    }

    for (size_t i = 0; i < routes_len; i++) {
        const apg_unit_node_source_t *from_node = routes[i].from.kind == APG_UNIT_ENDPOINT_NODE_FIELD
                                                      ? find_node_source(sources, sources_len, routes[i].from.node_id)
                                                      : NULL;
        const apg_unit_node_source_t *to_node   = routes[i].to.kind == APG_UNIT_ENDPOINT_NODE_FIELD
                                                      ? find_node_source(sources, sources_len, routes[i].to.node_id)
                                                      : NULL;
        if (routes[i].from.kind == APG_UNIT_ENDPOINT_NODE_FIELD && !from_node) {
            char msg[160];
            snprintf(msg, sizeof(msg), "graph route references unknown source node '%s'", routes[i].from.node_id);
            return set_error(err, UC_E_MISSING, msg);
        }
        if (routes[i].to.kind == APG_UNIT_ENDPOINT_NODE_FIELD && !to_node) {
            char msg[160];
            snprintf(msg, sizeof(msg), "graph route references unknown target node '%s'", routes[i].to.node_id);
            return set_error(err, UC_E_MISSING, msg);
        }

        const char *signal = route_signal_name(&routes[i], from_node, to_node, arena, err);
        if (!signal)
            return err->status;
        if (!signal_list_contains(out->signals, out->signals_len, signal)) {
            char msg[192];
            snprintf(msg, sizeof(msg), "graph route references unknown signal '%s'", signal);
            return set_error(err, UC_E_MISSING, msg);
        }
        apg_v2_value_t value = {APG_V2_VALUE_LITERAL, signal};

        if (from_node && !from_node->pseudo_input) {
            const char *key = resolve_route_field(from_node, routes[i].from.field, APG_ATOM_CONTRACT_OUT, err);
            if (!key)
                return err->status;
            apg_unit_v2_node_t *item = &items[from_node->output_index];
            uc_status           status =
                append_binding(item->out, &item->out_len, item->id, "out", key, value, routes[i].node, err);
            if (status != UC_OK)
                return status;
        }

        if (to_node && !to_node->pseudo_output) {
            const char *key = resolve_route_field(to_node, routes[i].to.field, APG_ATOM_CONTRACT_IN, err);
            if (!key)
                return err->status;
            apg_unit_v2_node_t *item = &items[to_node->output_index];
            uc_status status = append_binding(item->in, &item->in_len, item->id, "in", key, value, routes[i].node, err);
            if (status != UC_OK)
                return status;
        }
    }

    out->nodes     = items;
    out->nodes_len = normal_len;
    return UC_OK;
}

static uc_status validate_and_fill_graph(
    const uc_node *graph,
    const uc_node *ports,
    const uc_node *params,
    uc_arena      *arena,
    apg_unit_v2_t *out,
    uc_error      *err
) {
    if (!graph || graph->kind != UC_NODE_MAP)
        return set_error(err, UC_E_MISSING, "missing map field 'graph'");

    apg_unit_route_t *routes     = NULL;
    size_t            routes_len = 0u;
    uc_status         status     = fill_routes(uc_node_find(graph, "routes"), arena, &routes, &routes_len, err);
    if (status != UC_OK)
        return status;

    const uc_node *nodes = uc_node_find(graph, "nodes");
    if (routes_len == 0u && nodes && nodes->kind == UC_NODE_SEQ) {
        status =
            fill_signal_union(uc_node_find(graph, "signals"), ports, routes, routes_len, NULL, 0u, arena, out, err);
        if (status != UC_OK)
            return status;
        return validate_and_fill_nodes(nodes, params, arena, out, err);
    }

    apg_unit_node_source_t *sources     = NULL;
    size_t                  sources_len = 0u;
    size_t                  normal_len  = 0u;
    status                              = fill_node_sources(nodes, arena, &sources, &sources_len, &normal_len, err);
    if (status != UC_OK)
        return status;

    status = fill_signal_union(
        uc_node_find(graph, "signals"), ports, routes, routes_len, sources, sources_len, arena, out, err
    );
    if (status != UC_OK)
        return status;
    return validate_and_fill_routed_nodes(
        sources, sources_len, normal_len, routes, routes_len, params, arena, out, err
    );
}

static bool bool_scalar_is_valid(const uc_node *node) {
    const char *value = node_scalar(node);
    return value && (strcmp(value, "true") == 0 || strcmp(value, "false") == 0);
}

static uc_status
validate_and_fill_compatibility(const uc_node *compatibility, uc_arena *arena, apg_unit_v2_t *out, uc_error *err) {
    if (!compatibility)
        return set_error(err, UC_E_MISSING, "missing map field 'compatibility'");
    if (compatibility->kind != UC_NODE_MAP)
        return set_error(err, UC_E_TYPE, "compatibility must be a map");
    if (compatibility->map_len == 0)
        return set_error(err, UC_E_MISSING, "compatibility must declare at least one target");

    apg_unit_v2_compatibility_t *items = uc_arena_alloc(arena, compatibility->map_len * sizeof(*items), sizeof(void *));
    if (!items && compatibility->map_len > 0)
        return set_error(err, UC_E_OOM, "arena OOM");

    for (size_t i = 0; i < compatibility->map_len; i++) {
        const char *supported = node_scalar(compatibility->map[i].value);
        if (!bool_scalar_is_valid(compatibility->map[i].value))
            return set_error(err, UC_E_TYPE, "compatibility flag must be true or false");
        if (!apg_atom_profile_known(compatibility->map[i].key)) {
            char msg[128];
            snprintf(msg, sizeof(msg), "compatibility profile '%s' is unknown", compatibility->map[i].key);
            return set_error(err, UC_E_TYPE, msg);
        }
        items[i].target    = compatibility->map[i].key;
        items[i].supported = supported;
    }

    out->compatibility     = items;
    out->compatibility_len = compatibility->map_len;
    return UC_OK;
}

uc_status apg_unit_v2_validate_root(const uc_node *root, uc_arena *arena, apg_unit_v2_t *out, uc_error *err) {
    if (!root || root->kind != UC_NODE_MAP)
        return set_error(err, UC_E_TYPE, "root must be a map");

    if (!scalar_eq(uc_node_find(root, "kind"), "apg.unit"))
        return set_error(err, UC_E_TYPE, "kind must be 'apg.unit'");
    if (!scalar_eq(uc_node_find(root, "schema"), "apg.unit.v2"))
        return set_error(err, UC_E_TYPE, "schema must be 'apg.unit.v2'");

    out->name = required_scalar(root, "name", err);
    if (!out->name)
        return err->status;
    out->version = required_scalar(root, "version", err);
    if (!out->version)
        return err->status;

    uc_status status = validate_and_fill_meta(root, out, err);
    if (status != UC_OK)
        return status;
    status = validate_unit_ui(root, err);
    if (status != UC_OK)
        return status;

    const uc_node *params = uc_node_find(root, "params");
    status                = validate_and_fill_params(params, arena, out, err);
    if (status != UC_OK)
        return status;

    const uc_node *graph = uc_node_find(root, "graph");
    const uc_node *ports = uc_node_find(root, "ports");
    status               = validate_and_fill_graph(graph, ports, params, arena, out, err);
    if (status != UC_OK)
        return status;

    status = validate_and_fill_ports(ports, out->signals, out->signals_len, params, arena, out, err);
    if (status != UC_OK)
        return status;

    status = validate_and_fill_compatibility(uc_node_find(root, "compatibility"), arena, out, err);
    if (status != UC_OK)
        return status;

    return UC_OK;
}
