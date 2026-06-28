#include <apgcore/unit_v2.h>

#include <atom_registry.h>
#include <yaml/lexer.h>
#include <yaml/node.h>
#include <yaml/parser.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uc_status set_error(uc_error *err, uc_status status, const char *msg) {
    uc_loc loc = {0, 0};
    uc_error_set(err, status, loc, "%s", msg);
    return status;
}

static const char *node_scalar(const uc_node *node) {
    return node && node->kind == UC_NODE_SCALAR ? node->text : NULL;
}

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

static bool map_has_key(const uc_node *map, const char *key) {
    return uc_node_find(map, key) != NULL;
}

static bool seq_contains_scalar(const uc_node *seq, const char *value) {
    if (!seq || seq->kind != UC_NODE_SEQ || !value)
        return false;
    for (size_t i = 0; i < seq->seq_len; i++) {
        const uc_node *item = seq->seq[i];
        if (item && item->kind == UC_NODE_SCALAR && strcmp(item->text, value) == 0)
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

static bool port_type_is_audio(const char *type) {
    return type && strcmp(type, "audio") == 0;
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

static uc_status fill_bindings(
    const uc_node          *map,
    uc_arena              *arena,
    apg_unit_v2_binding_t **out_bindings,
    size_t                *out_len,
    uc_error              *err
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
            if (strcmp(map->map[j].key, map->map[i].key) == 0)
                return set_error(err, UC_E_RANGE, "duplicate node binding key");
        }
        bindings[i].key   = map->map[i].key;
        bindings[i].value = to_value(map->map[i].value);
    }

    *out_bindings = bindings;
    *out_len      = map->map_len;
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
            if (strcmp(params->map[j].key, params->map[i].key) == 0)
                return set_error(err, UC_E_RANGE, "duplicate param name");
        }

        const uc_node *param = params->map[i].value;
        if (!param || param->kind != UC_NODE_MAP)
            return set_error(err, UC_E_TYPE, "param entry must be a map");

        const char *type = required_scalar(param, "type", err);
        if (!type)
            return err->status;
        if (!param_type_is_valid(type))
            return set_error(err, UC_E_TYPE, "param type must be 'float', 'int', or 'bool'");
        const char *default_value = value_text(uc_node_find(param, "default"));
        const char *min_value     = value_text(uc_node_find(param, "min"));
        const char *max_value     = value_text(uc_node_find(param, "max"));
        if (!default_value)
            return set_error(err, UC_E_MISSING, "param missing 'default'");
        if (param_type_is_numeric(type)) {
            if (!min_value)
                return set_error(err, UC_E_MISSING, "numeric param missing 'min'");
            if (!max_value)
                return set_error(err, UC_E_MISSING, "numeric param missing 'max'");
        }

        items[i].name          = params->map[i].key;
        items[i].type          = type;
        items[i].default_value = default_value;
        items[i].min_value     = min_value;
        items[i].max_value     = max_value;
        items[i].smoothing_ms  = value_text(uc_node_find(param, "smoothing_ms"));
    }

    out->params     = items;
    out->params_len = params->map_len;
    return UC_OK;
}

static uc_status fill_port_group(
    const uc_node        *seq,
    const uc_node        *signals,
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
            if (strcmp(ports[j].name, name) == 0)
                return set_error(err, UC_E_RANGE, "duplicate port name");
        }
        const char *type = required_scalar(port, "type", err);
        if (!type)
            return err->status;
        if (!port_type_is_valid(type))
            return set_error(err, UC_E_TYPE, "port type must be 'audio' or 'control'");
        const char *channels = value_text(uc_node_find(port, "channels"));
        if (port_type_is_audio(type)) {
            if (!channels)
                return set_error(err, UC_E_MISSING, "audio port missing 'channels'");
            if (!seq_contains_scalar(signals, name))
                return set_error(err, UC_E_MISSING, "public audio port is missing matching graph signal");
        }

        ports[i].name     = name;
        ports[i].type     = type;
        ports[i].channels = channels;
    }

    *out_ports = ports;
    *out_len   = seq->seq_len;
    return UC_OK;
}

static uc_status validate_and_fill_ports(const uc_node *ports, const uc_node *signals, uc_arena *arena, apg_unit_v2_t *out, uc_error *err) {
    if (!ports || ports->kind != UC_NODE_MAP)
        return set_error(err, UC_E_MISSING, "missing map field 'ports'");

    const uc_node *inputs  = uc_node_find(ports, "inputs");
    const uc_node *outputs = uc_node_find(ports, "outputs");
    uc_status      status  = fill_port_group(inputs, signals, arena, &out->input_ports, &out->input_ports_len, err);
    if (status != UC_OK)
        return status;
    return fill_port_group(outputs, signals, arena, &out->output_ports, &out->output_ports_len, err);
}

static uc_status fill_signals(const uc_node *signals, uc_arena *arena, apg_unit_v2_t *out, uc_error *err) {
    if (!signals || signals->kind != UC_NODE_SEQ)
        return set_error(err, UC_E_MISSING, "missing graph.signals");

    const char **items = uc_arena_alloc(arena, signals->seq_len * sizeof(*items), sizeof(void *));
    if (!items && signals->seq_len > 0)
        return set_error(err, UC_E_OOM, "arena OOM");

    for (size_t i = 0; i < signals->seq_len; i++) {
        const char *signal = node_scalar(signals->seq[i]);
        if (!signal)
            return set_error(err, UC_E_TYPE, "graph signal must be a scalar");
        for (size_t j = 0; j < i; j++) {
            if (strcmp(items[j], signal) == 0)
                return set_error(err, UC_E_RANGE, "duplicate graph signal name");
        }
        items[i] = signal;
    }

    out->signals     = items;
    out->signals_len = signals->seq_len;
    return UC_OK;
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
            if (b_id && b_id->kind == UC_NODE_SCALAR && strcmp(a_id, b_id->text) == 0)
                return set_error(err, UC_E_RANGE, "duplicate graph node id");
        }
    }
    return UC_OK;
}

static uc_status validate_and_fill_nodes(
    const uc_node *nodes,
    const uc_node *params,
    uc_arena     *arena,
    apg_unit_v2_t *out,
    uc_error     *err
) {
    if (!nodes || nodes->kind != UC_NODE_SEQ)
        return set_error(err, UC_E_MISSING, "missing graph.nodes");

    uc_status status = validate_node_ids_unique(nodes, err);
    if (status != UC_OK)
        return status;

    apg_unit_v2_node_t *items = uc_arena_alloc(arena, nodes->seq_len * sizeof(*items), sizeof(void *));
    if (!items && nodes->seq_len > 0)
        return set_error(err, UC_E_OOM, "arena OOM");

    atom_registry_init();
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
        if (!atom_registry_find(atom))
            return set_error(err, UC_E_MISSING, "unknown graph node atom");

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

static uc_status validate_and_fill_graph(const uc_node *graph, const uc_node *params, uc_arena *arena, apg_unit_v2_t *out, uc_error *err) {
    if (!graph || graph->kind != UC_NODE_MAP)
        return set_error(err, UC_E_MISSING, "missing map field 'graph'");

    uc_status status = fill_signals(uc_node_find(graph, "signals"), arena, out, err);
    if (status != UC_OK)
        return status;

    return validate_and_fill_nodes(uc_node_find(graph, "nodes"), params, arena, out, err);
}

static bool bool_scalar_is_valid(const uc_node *node) {
    const char *value = node_scalar(node);
    return value && (strcmp(value, "true") == 0 || strcmp(value, "false") == 0);
}

static uc_status validate_compatibility(const uc_node *compatibility, uc_error *err) {
    if (!compatibility)
        return set_error(err, UC_E_MISSING, "missing map field 'compatibility'");
    if (compatibility->kind != UC_NODE_MAP)
        return set_error(err, UC_E_TYPE, "compatibility must be a map");
    if (compatibility->map_len == 0)
        return set_error(err, UC_E_MISSING, "compatibility must declare at least one target");

    for (size_t i = 0; i < compatibility->map_len; i++) {
        if (!bool_scalar_is_valid(compatibility->map[i].value))
            return set_error(err, UC_E_TYPE, "compatibility flag must be true or false");
    }
    return UC_OK;
}

static uc_status validate_unit_root(const uc_node *root, uc_arena *arena, apg_unit_v2_t *out, uc_error *err) {
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

    const uc_node *params = uc_node_find(root, "params");
    uc_status      status = validate_and_fill_params(params, arena, out, err);
    if (status != UC_OK)
        return status;

    const uc_node *graph = uc_node_find(root, "graph");
    status               = validate_and_fill_graph(graph, params, arena, out, err);
    if (status != UC_OK)
        return status;

    status = validate_and_fill_ports(uc_node_find(root, "ports"), uc_node_find(graph, "signals"), arena, out, err);
    if (status != UC_OK)
        return status;

    status = validate_compatibility(uc_node_find(root, "compatibility"), err);
    if (status != UC_OK)
        return status;

    return UC_OK;
}

uc_status apg_unit_v2_load_string(const char *src, size_t src_len, uc_arena *arena, apg_unit_v2_t *out, uc_error *err) {
    if (!src || !arena || !out || !err)
        return UC_E_TYPE;
    memset(out, 0, sizeof(*out));
    err->status = UC_OK;

    uc_token_vec tokens = {0};
    uc_status    status = uc_lex(src, src_len, arena, &tokens, err);
    if (status != UC_OK)
        return status;

    uc_node *root = NULL;
    status        = uc_parse(&tokens, arena, &root, err);
    if (status != UC_OK)
        return status;

    return validate_unit_root(root, arena, out, err);
}

uc_status apg_unit_v2_load_file(const char *path, uc_arena *arena, apg_unit_v2_t *out, uc_error *err) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        uc_loc loc = {0, 0};
        uc_error_set(err, UC_E_IO, loc, "cannot open '%s'", path);
        return UC_E_IO;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (size < 0) {
        fclose(file);
        return set_error(err, UC_E_IO, "ftell failed");
    }

    char *buffer = (char *)uc_arena_alloc(arena, (size_t)size + 1u, 1u);
    if (!buffer) {
        fclose(file);
        return set_error(err, UC_E_OOM, "arena OOM");
    }
    if (fread(buffer, 1u, (size_t)size, file) != (size_t)size) {
        fclose(file);
        return set_error(err, UC_E_IO, "fread failed");
    }
    buffer[size] = '\0';
    fclose(file);

    return apg_unit_v2_load_string(buffer, (size_t)size, arena, out, err);
}
