#include <apgcore/compiler/compiler_v2.h>

#include <apgcore/metadata/atom_catalog.h>

#include <yaml/node.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    APG_BIND_SECTION_IN,
    APG_BIND_SECTION_OUT,
    APG_BIND_SECTION_CONFIG,
} apg_bind_section_t;

static uc_status set_error(uc_error *err, uc_status status, const char *msg) {
    uc_loc loc = {0, 0};
    uc_error_set(err, status, loc, "%s", msg);
    return status;
}

static const char *bind_section_name(apg_bind_section_t section) {
    switch (section) {
    case APG_BIND_SECTION_IN:
        return "in";
    case APG_BIND_SECTION_OUT:
        return "out";
    case APG_BIND_SECTION_CONFIG:
        return "config";
    }
    return "binding";
}

static const char *param_ref_name(const char *text) {
    if (!text)
        return NULL;
    return strncmp(text, "params.", 7) == 0 ? text + 7 : NULL;
}

static int find_signal_index(const apg_unit_v2_t *unit, const char *name) {
    if (!unit || !name)
        return -1;
    for (size_t i = 0; i < unit->signals_len; i++) {
        if (unit->signals[i] && strcmp(unit->signals[i], name) == 0)
            return (int)i;
    }
    return -1;
}

static int find_param_index(const apg_unit_v2_t *unit, const char *name) {
    if (!unit || !name)
        return -1;
    for (size_t i = 0; i < unit->params_len; i++) {
        if (unit->params[i].name && strcmp(unit->params[i].name, name) == 0)
            return (int)i;
    }
    return -1;
}

static int port_is_audio(const apg_unit_v2_port_t *port) {
    return port && port->type && strcmp(port->type, "audio") == 0;
}

static size_t port_signal_count(const apg_unit_v2_port_t *port) {
    return port && port->signals_len > 0u ? port->signals_len : 1u;
}

static const char *port_signal_name(const apg_unit_v2_port_t *port, size_t channel_index) {
    if (!port)
        return NULL;
    if (port->signals_len > 0u)
        return channel_index < port->signals_len ? port->signals[channel_index] : NULL;
    return channel_index == 0u ? port->name : NULL;
}

static int node_id_has_instance_prefix_len(const char *node_id, const char *instance_id, size_t instance_len) {
    return node_id && instance_id && instance_id[0] != '\0' && strncmp(node_id, instance_id, instance_len) == 0 &&
           (node_id[instance_len] == '\0' || node_id[instance_len] == '.');
}

static int node_instance_prefix(const char *node_id, const char **out_instance_id, size_t *out_instance_len) {
    if (!node_id || node_id[0] == '\0' || !out_instance_id || !out_instance_len)
        return 0;
    const char *dot = strchr(node_id, '.');
    if (dot) {
        size_t instance_len = (size_t)(dot - node_id);
        if (instance_len == 0u)
            return 0;
        *out_instance_id  = node_id;
        *out_instance_len = instance_len;
        return 1;
    }
    *out_instance_id  = node_id;
    *out_instance_len = strlen(node_id);
    return 1;
}

static size_t binding_signal_count(const apg_v2_compiled_binding_t *binding) {
    if (!binding)
        return 0u;
    if (binding->kind == APG_BIND_SIGNAL)
        return 1u;
    if (binding->kind == APG_BIND_SIGNAL_ARRAY)
        return binding->indices_len;
    return 0u;
}

static int binding_signal_index(const apg_v2_compiled_binding_t *binding, size_t offset, size_t *out_index) {
    if (!binding || !out_index)
        return 0;
    if (binding->kind == APG_BIND_SIGNAL && offset == 0u) {
        *out_index = binding->index;
        return 1;
    }
    if (binding->kind == APG_BIND_SIGNAL_ARRAY && offset < binding->indices_len) {
        *out_index = binding->indices[offset];
        return 1;
    }
    return 0;
}

static uc_status mark_input_port_signals(const apg_unit_v2_t *unit, int *signal_available, uc_error *err) {
    for (size_t i = 0; i < unit->input_ports_len; i++) {
        if (!port_is_audio(&unit->input_ports[i]))
            continue;
        for (size_t ch = 0; ch < port_signal_count(&unit->input_ports[i]); ch++) {
            int signal_index = find_signal_index(unit, port_signal_name(&unit->input_ports[i], ch));
            if (signal_index < 0)
                return set_error(err, UC_E_MISSING, "input audio port is missing graph signal mapping");
            signal_available[signal_index] = 1;
        }
    }
    return UC_OK;
}

static int node_inputs_available(const apg_v2_compiled_node_t *node, const int *signal_available) {
    for (size_t i = 0; i < node->in_len; i++) {
        if (node->in[i].kind == APG_BIND_SIGNAL && !signal_available[node->in[i].index])
            return 0;
        if (node->in[i].kind == APG_BIND_SIGNAL_ARRAY) {
            for (size_t j = 0; j < node->in[i].indices_len; j++) {
                if (!signal_available[node->in[i].indices[j]])
                    return 0;
            }
        }
    }
    return 1;
}

static void mark_node_outputs_available(const apg_v2_compiled_node_t *node, int *signal_available) {
    for (size_t i = 0; i < node->out_len; i++) {
        if (node->out[i].kind == APG_BIND_SIGNAL)
            signal_available[node->out[i].index] = 1;
        else if (node->out[i].kind == APG_BIND_SIGNAL_ARRAY) {
            for (size_t j = 0; j < node->out[i].indices_len; j++)
                signal_available[node->out[i].indices[j]] = 1;
        }
    }
}

static uc_status record_node_output_producers(
    const apg_v2_compiled_node_t *node, uint32_t node_index, uint32_t *signal_producers, uc_error *err
) {
    for (size_t i = 0; i < node->out_len; i++) {
        size_t signal_count = node->out[i].kind == APG_BIND_SIGNAL_ARRAY ? node->out[i].indices_len : 1u;
        for (size_t j = 0; j < signal_count; j++) {
            if (node->out[i].kind != APG_BIND_SIGNAL && node->out[i].kind != APG_BIND_SIGNAL_ARRAY)
                continue;
            size_t signal_index =
                node->out[i].kind == APG_BIND_SIGNAL_ARRAY ? node->out[i].indices[j] : node->out[i].index;
            if (signal_producers[signal_index] != UINT32_MAX) {
                char msg[160];
                snprintf(
                    msg, sizeof(msg), "node '%s' out binding '%s' writes signal with multiple producers",
                    node->id ? node->id : "", node->out[i].key ? node->out[i].key : ""
                );
                return set_error(err, UC_E_MISSING, msg);
            }
            signal_producers[signal_index] = node_index;
        }
    }
    return UC_OK;
}

static int node_outputs_signal(const apg_v2_compiled_node_t *node, size_t signal_index) {
    if (!node)
        return 0;
    for (size_t i = 0; i < node->out_len; i++) {
        for (size_t j = 0; j < binding_signal_count(&node->out[i]); j++) {
            size_t index = 0u;
            if (binding_signal_index(&node->out[i], j, &index) && index == signal_index)
                return 1;
        }
    }
    return 0;
}

static int instance_outputs_signal(
    const apg_v2_compiled_node_t *nodes,
    size_t                        nodes_len,
    const char                   *instance_id,
    size_t                        instance_len,
    size_t                        signal_index
) {
    for (size_t i = 0; nodes && i < nodes_len; i++) {
        if (node_id_has_instance_prefix_len(nodes[i].id, instance_id, instance_len) &&
            node_outputs_signal(&nodes[i], signal_index))
            return 1;
    }
    return 0;
}

static int signal_consumed_outside_instance(
    const apg_v2_compiled_node_t *nodes,
    size_t                        nodes_len,
    const char                   *instance_id,
    size_t                        instance_len,
    size_t                        signal_index
) {
    for (size_t i = 0; nodes && i < nodes_len; i++) {
        if (node_id_has_instance_prefix_len(nodes[i].id, instance_id, instance_len))
            continue;
        for (size_t j = 0; j < nodes[i].in_len; j++) {
            for (size_t k = 0; k < binding_signal_count(&nodes[i].in[j]); k++) {
                size_t index = 0u;
                if (binding_signal_index(&nodes[i].in[j], k, &index) && index == signal_index)
                    return 1;
            }
        }
    }
    return 0;
}

static uc_status
validate_output_port_signals_produced(const apg_unit_v2_t *unit, const int *signal_available, uc_error *err) {
    for (size_t i = 0; i < unit->output_ports_len; i++) {
        if (!port_is_audio(&unit->output_ports[i]))
            continue;
        for (size_t ch = 0; ch < port_signal_count(&unit->output_ports[i]); ch++) {
            int signal_index = find_signal_index(unit, port_signal_name(&unit->output_ports[i], ch));
            if (signal_index < 0)
                return set_error(err, UC_E_MISSING, "output audio port is missing graph signal mapping");
            if (!signal_available[signal_index])
                return set_error(err, UC_E_MISSING, "output audio port signal is not produced by the graph");
        }
    }
    return UC_OK;
}

static int signal_is_public_output(const apg_unit_v2_t *unit, size_t signal_index) {
    if (!unit)
        return 0;
    for (size_t i = 0; i < unit->output_ports_len; i++) {
        if (!port_is_audio(&unit->output_ports[i]))
            continue;
        for (size_t ch = 0; ch < port_signal_count(&unit->output_ports[i]); ch++) {
            int index = find_signal_index(unit, port_signal_name(&unit->output_ports[i], ch));
            if (index >= 0 && (size_t)index == signal_index)
                return 1;
        }
    }
    return 0;
}

static void fill_instance_io(
    const apg_unit_v2_t          *unit,
    const apg_v2_compiled_node_t *nodes,
    size_t                        nodes_len,
    apg_v2_compiled_instance_t   *instance
) {
    instance->input_signal_index  = (size_t)-1u;
    instance->output_signal_index = (size_t)-1u;
    instance->bypassable          = false;

    for (size_t i = 0; nodes && i < nodes_len && instance->input_signal_index == (size_t)-1u; i++) {
        if (!node_id_has_instance_prefix_len(nodes[i].id, instance->id, instance->id_len))
            continue;
        for (size_t j = 0; j < nodes[i].in_len && instance->input_signal_index == (size_t)-1u; j++) {
            for (size_t k = 0; k < binding_signal_count(&nodes[i].in[j]); k++) {
                size_t index = 0u;
                if (!binding_signal_index(&nodes[i].in[j], k, &index))
                    continue;
                if (!instance_outputs_signal(nodes, nodes_len, instance->id, instance->id_len, index)) {
                    instance->input_signal_index = index;
                    break;
                }
            }
        }
    }

    for (size_t i = 0; nodes && i < nodes_len && instance->output_signal_index == (size_t)-1u; i++) {
        if (!node_id_has_instance_prefix_len(nodes[i].id, instance->id, instance->id_len))
            continue;
        for (size_t j = 0; j < nodes[i].out_len && instance->output_signal_index == (size_t)-1u; j++) {
            for (size_t k = 0; k < binding_signal_count(&nodes[i].out[j]); k++) {
                size_t index = 0u;
                if (!binding_signal_index(&nodes[i].out[j], k, &index))
                    continue;
                if (signal_consumed_outside_instance(nodes, nodes_len, instance->id, instance->id_len, index) ||
                    signal_is_public_output(unit, index)) {
                    instance->output_signal_index = index;
                    break;
                }
            }
        }
    }

    instance->bypassable = instance->input_signal_index != (size_t)-1u && instance->output_signal_index != (size_t)-1u;
}

static size_t find_compiled_instance(
    const apg_v2_compiled_instance_t *instances, size_t instances_len, const char *id, size_t id_len
) {
    for (size_t i = 0; instances && i < instances_len; i++) {
        if (instances[i].id_len == id_len && strncmp(instances[i].id, id, id_len) == 0)
            return i;
    }
    return (size_t)-1u;
}

static uc_status build_instance_metadata(
    const apg_unit_v2_t          *unit,
    const apg_v2_compiled_node_t *nodes,
    size_t                        nodes_len,
    uc_arena                     *arena,
    apg_v2_compiled_unit_t       *out,
    uc_error                     *err
) {
    out->instances                  = NULL;
    out->instances_len              = 0u;
    out->instance_index_by_node     = NULL;
    out->instance_index_by_node_len = nodes_len;
    if (nodes_len == 0u)
        return UC_OK;

    apg_v2_compiled_instance_t *instances = uc_arena_alloc(arena, nodes_len * sizeof(*instances), sizeof(void *));
    size_t                     *by_node   = uc_arena_alloc(arena, nodes_len * sizeof(*by_node), sizeof(size_t));
    if (!instances || !by_node)
        return set_error(err, UC_E_OOM, "arena OOM");

    size_t instances_len = 0u;
    for (size_t i = 0; i < nodes_len; i++) {
        by_node[i]               = (size_t)-1u;
        const char *instance_id  = NULL;
        size_t      instance_len = 0u;
        if (!node_instance_prefix(nodes[i].id, &instance_id, &instance_len))
            continue;

        size_t instance_index = find_compiled_instance(instances, instances_len, instance_id, instance_len);
        if (instance_index == (size_t)-1u) {
            instance_index            = instances_len++;
            instances[instance_index] = (apg_v2_compiled_instance_t){.id = instance_id, .id_len = instance_len};
        }
        by_node[i] = instance_index;
    }

    for (size_t i = 0; i < instances_len; i++)
        fill_instance_io(unit, nodes, nodes_len, &instances[i]);

    out->instances              = instances;
    out->instances_len          = instances_len;
    out->instance_index_by_node = by_node;
    return UC_OK;
}

static uc_status build_schedule(
    const apg_v2_compiled_node_t *nodes,
    size_t                        nodes_len,
    uint32_t                     *schedule,
    int                          *node_scheduled,
    int                          *signal_available,
    uc_error                     *err
) {
    size_t scheduled_len = 0;
    while (scheduled_len < nodes_len) {
        int progressed = 0;
        for (size_t i = 0; i < nodes_len; i++) {
            if (node_scheduled[i] || !node_inputs_available(&nodes[i], signal_available))
                continue;
            schedule[scheduled_len++] = (uint32_t)i;
            node_scheduled[i]         = 1;
            mark_node_outputs_available(&nodes[i], signal_available);
            progressed = 1;
        }
        if (!progressed)
            return set_error(err, UC_E_MISSING, "graph contains unresolved dependency or direct cycle");
    }
    return UC_OK;
}

static apg_atom_contract_section_t contract_section(apg_bind_section_t section) {
    switch (section) {
    case APG_BIND_SECTION_IN:
        return APG_ATOM_CONTRACT_IN;
    case APG_BIND_SECTION_OUT:
        return APG_ATOM_CONTRACT_OUT;
    case APG_BIND_SECTION_CONFIG:
        return APG_ATOM_CONTRACT_CONFIG;
    }
    return APG_ATOM_CONTRACT_IN;
}

static int atom_has_contract(const char *atom) {
    return apg_atom_contract_field_count(atom, APG_ATOM_CONTRACT_IN) > 0u ||
           apg_atom_contract_field_count(atom, APG_ATOM_CONTRACT_OUT) > 0u ||
           apg_atom_contract_field_count(atom, APG_ATOM_CONTRACT_CONFIG) > 0u;
}

static int atom_binding_key_allowed(const char *atom, apg_bind_section_t section, const char *key) {
    if (!atom_has_contract(atom))
        return 1;
    return apg_atom_contract_find_field(atom, contract_section(section), key, NULL);
}

static int atom_binding_key_required(const char *atom, apg_bind_section_t section, const char *key) {
    return apg_atom_contract_field_required(atom, contract_section(section), key);
}

static int atom_binding_key_is_type(
    const char *atom, apg_bind_section_t section, const char *key, apg_atom_contract_field_type_t type
) {
    return apg_atom_contract_field_type(atom, contract_section(section), key) == type;
}

static int atom_input_key_is_scalar(const char *atom, const char *key) {
    return atom_binding_key_is_type(atom, APG_BIND_SECTION_IN, key, APG_ATOM_FIELD_SCALAR);
}

static int atom_signal_key_is_array(const char *atom, apg_bind_section_t section, const char *key) {
    return atom_binding_key_is_type(atom, section, key, APG_ATOM_FIELD_SIGNAL_ARRAY);
}

static int atom_config_key_is_float_matrix(const char *atom, const char *key) {
    return atom_binding_key_is_type(atom, APG_BIND_SECTION_CONFIG, key, APG_ATOM_FIELD_FLOAT_MATRIX);
}

static void init_compiled_binding(apg_v2_compiled_binding_t *binding, const char *key) {
    memset(binding, 0, sizeof(*binding));
    binding->key = key;
}

static uc_status validate_binding_key(
    const char *node_id, const char *atom, apg_bind_section_t section, const char *key, uc_error *err
) {
    if (atom_binding_key_allowed(atom, section, key))
        return UC_OK;

    char msg[192];
    snprintf(
        msg, sizeof(msg), "node '%s' atom '%s' %s binding key '%s' is not accepted", node_id ? node_id : "",
        atom ? atom : "", bind_section_name(section), key ? key : ""
    );
    return set_error(err, UC_E_MISSING, msg);
}

static int binding_key_present(const apg_unit_v2_binding_t *bindings, size_t bindings_len, const char *key) {
    for (size_t i = 0; i < bindings_len; i++) {
        if (bindings[i].key && strcmp(bindings[i].key, key) == 0)
            return 1;
    }
    return 0;
}

static uc_status validate_required_binding_keys(
    const char                  *node_id,
    const char                  *atom,
    apg_bind_section_t           section,
    const apg_unit_v2_binding_t *bindings,
    size_t                       bindings_len,
    uc_error                    *err
) {
    apg_atom_contract_section_t contract   = contract_section(section);
    size_t                      fields_len = apg_atom_contract_field_count(atom, contract);
    if (fields_len == 0u)
        return UC_OK;

    for (size_t i = 0; i < fields_len; i++) {
        apg_atom_contract_field_t field;
        if (!apg_atom_contract_field(atom, contract, i, &field) || !field.required)
            continue;
        if (!binding_key_present(bindings, bindings_len, field.name)) {
            char msg[192];
            snprintf(
                msg, sizeof(msg), "node '%s' atom '%s' is missing required %s binding key '%s'", node_id ? node_id : "",
                atom ? atom : "", bind_section_name(section), field.name ? field.name : ""
            );
            return set_error(err, UC_E_MISSING, msg);
        }
    }
    return UC_OK;
}

static const char *binding_node_scalar(const uc_node *node) {
    return node && node->kind == UC_NODE_SCALAR ? node->text : NULL;
}

static uc_status compile_signal_array_binding(
    const apg_unit_v2_t         *unit,
    const char                  *node_id,
    apg_bind_section_t           section,
    const apg_unit_v2_binding_t *binding,
    uc_arena                    *arena,
    apg_v2_compiled_binding_t   *out,
    uc_error                    *err
) {
    if (!binding->node || binding->node->kind != UC_NODE_SEQ || binding->node->seq_len == 0u) {
        char msg[192];
        snprintf(
            msg, sizeof(msg), "node '%s' %s binding key '%s' must be a non-empty signal array", node_id ? node_id : "",
            bind_section_name(section), binding->key ? binding->key : ""
        );
        return set_error(err, UC_E_TYPE, msg);
    }

    size_t *indices = uc_arena_alloc(arena, binding->node->seq_len * sizeof(*indices), sizeof(size_t));
    if (!indices)
        return set_error(err, UC_E_OOM, "arena OOM");

    for (size_t i = 0; i < binding->node->seq_len; i++) {
        const char *signal = binding_node_scalar(binding->node->seq[i]);
        int         index  = find_signal_index(unit, signal);
        if (index < 0) {
            char msg[192];
            snprintf(
                msg, sizeof(msg), "node '%s' %s binding key '%s' references unknown signal '%s'",
                node_id ? node_id : "", bind_section_name(section), binding->key ? binding->key : "",
                signal ? signal : ""
            );
            return set_error(err, UC_E_MISSING, msg);
        }
        indices[i] = (size_t)index;
    }

    out->kind        = APG_BIND_SIGNAL_ARRAY;
    out->indices     = indices;
    out->indices_len = binding->node->seq_len;
    return UC_OK;
}

static int parse_float_literal(const char *text, float *out_value) {
    if (!text || !out_value)
        return 0;
    char *end   = NULL;
    float value = strtof(text, &end);
    if (!end || *end != '\0')
        return 0;
    *out_value = value;
    return 1;
}

static uc_status compile_scalar_literal(
    const char                  *node_id,
    apg_bind_section_t           section,
    apg_v2_compiled_binding_t   *out,
    const apg_unit_v2_binding_t *binding,
    uc_error                    *err
) {
    float value = 0.0f;
    if (!parse_float_literal(binding ? binding->value.text : NULL, &value)) {
        char msg[192];
        snprintf(
            msg, sizeof(msg), "node '%s' %s binding key '%s' must be a numeric literal", node_id ? node_id : "",
            bind_section_name(section), binding && binding->key ? binding->key : ""
        );
        return set_error(err, UC_E_TYPE, msg);
    }
    out->kind    = APG_BIND_LITERAL;
    out->literal = binding->value.text;
    out->number  = value;
    return UC_OK;
}

static size_t matrix_row_count(const uc_node *node) {
    if (!node)
        return 0u;
    if (node->kind == UC_NODE_SEQ)
        return node->seq_len;
    if (node->kind == UC_NODE_MAP)
        return node->map_len;
    return 0u;
}

static const uc_node *matrix_row_node(const uc_node *node, size_t row) {
    if (!node)
        return NULL;
    if (node->kind == UC_NODE_SEQ)
        return row < node->seq_len ? node->seq[row] : NULL;
    if (node->kind == UC_NODE_MAP)
        return row < node->map_len ? node->map[row].value : NULL;
    return NULL;
}

static size_t matrix_col_count(const uc_node *row_node) {
    if (!row_node)
        return 0u;
    if (row_node->kind == UC_NODE_SEQ)
        return row_node->seq_len;
    if (row_node->kind == UC_NODE_MAP)
        return row_node->map_len;
    return 0u;
}

static const uc_node *matrix_cell_node(const uc_node *row_node, size_t col) {
    if (!row_node)
        return NULL;
    if (row_node->kind == UC_NODE_SEQ)
        return col < row_node->seq_len ? row_node->seq[col] : NULL;
    if (row_node->kind == UC_NODE_MAP)
        return col < row_node->map_len ? row_node->map[col].value : NULL;
    return NULL;
}

static uc_status compile_float_matrix_binding(
    const char                  *node_id,
    const apg_unit_v2_binding_t *binding,
    uc_arena                    *arena,
    apg_v2_compiled_binding_t   *out,
    uc_error                    *err
) {
    size_t rows = matrix_row_count(binding ? binding->node : NULL);
    if (rows == 0u)
        return set_error(err, UC_E_TYPE, "mix_matrix coefficients must be a non-empty matrix");

    size_t cols = 0;
    for (size_t row = 0; row < rows; row++) {
        const uc_node *row_node = matrix_row_node(binding->node, row);
        size_t         row_cols = matrix_col_count(row_node);
        if (row_cols == 0u)
            return set_error(err, UC_E_TYPE, "mix_matrix coefficients rows must be non-empty sequences or maps");
        if (row == 0u)
            cols = row_cols;
        else if (row_cols != cols) {
            char msg[192];
            snprintf(msg, sizeof(msg), "node '%s' mix_matrix coefficients must be rectangular", node_id ? node_id : "");
            return set_error(err, UC_E_RANGE, msg);
        }
    }

    float *numbers = uc_arena_alloc(arena, rows * cols * sizeof(*numbers), sizeof(float));
    if (!numbers)
        return set_error(err, UC_E_OOM, "arena OOM");

    for (size_t row = 0; row < rows; row++) {
        const uc_node *row_node = matrix_row_node(binding->node, row);
        for (size_t col = 0; col < cols; col++) {
            float value = 0.0f;
            if (!parse_float_literal(binding_node_scalar(matrix_cell_node(row_node, col)), &value)) {
                char msg[192];
                snprintf(msg, sizeof(msg), "node '%s' mix_matrix coefficient must be numeric", node_id ? node_id : "");
                return set_error(err, UC_E_TYPE, msg);
            }
            numbers[row * cols + col] = value;
        }
    }

    out->kind    = APG_BIND_FLOAT_MATRIX;
    out->numbers = numbers;
    out->rows    = rows;
    out->cols    = cols;
    return UC_OK;
}

static const apg_v2_compiled_binding_t *
find_compiled_binding(const apg_v2_compiled_binding_t *bindings, size_t bindings_len, const char *key) {
    for (size_t i = 0; i < bindings_len; i++) {
        if (bindings[i].key && key && strcmp(bindings[i].key, key) == 0)
            return &bindings[i];
    }
    return NULL;
}

static uc_status validate_mix_matrix_shape(const apg_v2_compiled_node_t *node, uc_error *err) {
    if (!node || !node->atom_name || strcmp(node->atom_name, "mix_matrix") != 0)
        return UC_OK;

    const apg_v2_compiled_binding_t *in     = find_compiled_binding(node->in, node->in_len, "signals");
    const apg_v2_compiled_binding_t *out    = find_compiled_binding(node->out, node->out_len, "signals");
    const apg_v2_compiled_binding_t *matrix = find_compiled_binding(node->config, node->config_len, "coefficients");
    if (!in || !out || !matrix || in->kind != APG_BIND_SIGNAL_ARRAY || out->kind != APG_BIND_SIGNAL_ARRAY ||
        matrix->kind != APG_BIND_FLOAT_MATRIX)
        return set_error(err, UC_E_TYPE, "mix_matrix requires signal arrays and coefficient matrix bindings");
    if (in->indices_len != matrix->cols || out->indices_len != matrix->rows) {
        char msg[192];
        snprintf(
            msg, sizeof(msg), "node '%s' mix_matrix shape must match input/output signal counts",
            node->id ? node->id : ""
        );
        return set_error(err, UC_E_RANGE, msg);
    }
    return UC_OK;
}

static void populate_atom_layout(apg_v2_compiled_node_t *node, const atom_registry_entry_t *atom) {
    size_t input_fields_len = 0u;

    node->atom              = atom;
    node->atom_name         = atom->name;
    node->thunk             = atom->thunk;
    node->out_size          = atom->out_size;
    node->in_size           = atom->in_size;
    node->config_size       = atom->config_size;
    node->state_size        = atom->state_size;
    node->input_fields      = atom_registry_in_fields(atom, &input_fields_len);
    node->input_fields_len  = input_fields_len;
    node->config_fields     = atom->config_fields;
    node->config_fields_len = atom->n_config_fields > 0 ? (size_t)atom->n_config_fields : 0u;
    node->state_fields      = atom->state_fields;
    node->state_fields_len  = atom->n_state_fields > 0 ? (size_t)atom->n_state_fields : 0u;
}

static uc_status compile_signal_bindings(
    const apg_unit_v2_t         *unit,
    const char                  *node_id,
    const char                  *atom,
    apg_bind_section_t           section,
    const apg_unit_v2_binding_t *bindings,
    size_t                       bindings_len,
    uc_arena                    *arena,
    apg_v2_compiled_binding_t  **out_bindings,
    size_t                      *out_len,
    uc_error                    *err
) {
    *out_bindings    = NULL;
    *out_len         = 0;
    uc_status status = validate_required_binding_keys(node_id, atom, section, bindings, bindings_len, err);
    if (status != UC_OK)
        return status;
    if (bindings_len == 0)
        return UC_OK;

    apg_v2_compiled_binding_t *compiled = uc_arena_alloc(arena, bindings_len * sizeof(*compiled), sizeof(void *));
    if (!compiled)
        return set_error(err, UC_E_OOM, "arena OOM");

    for (size_t i = 0; i < bindings_len; i++) {
        status = validate_binding_key(node_id, atom, section, bindings[i].key, err);
        if (status != UC_OK)
            return status;
        init_compiled_binding(&compiled[i], bindings[i].key);

        if (atom_signal_key_is_array(atom, section, bindings[i].key)) {
            status = compile_signal_array_binding(unit, node_id, section, &bindings[i], arena, &compiled[i], err);
            if (status != UC_OK)
                return status;
            continue;
        }

        if (section == APG_BIND_SECTION_IN && atom_input_key_is_scalar(atom, bindings[i].key)) {
            if (bindings[i].value.kind == APG_V2_VALUE_VARREF) {
                const char *param_name  = param_ref_name(bindings[i].value.text);
                int         param_index = find_param_index(unit, param_name);
                if (param_index < 0) {
                    char msg[192];
                    snprintf(
                        msg, sizeof(msg), "node '%s' in binding key '%s' references unknown parameter '%s'",
                        node_id ? node_id : "", bindings[i].key ? bindings[i].key : "",
                        bindings[i].value.text ? bindings[i].value.text : ""
                    );
                    return set_error(err, UC_E_MISSING, msg);
                }
                compiled[i].kind  = APG_BIND_PARAM;
                compiled[i].index = (size_t)param_index;
            } else {
                status = compile_scalar_literal(node_id, section, &compiled[i], &bindings[i], err);
                if (status != UC_OK)
                    return status;
            }
            continue;
        }

        if (bindings[i].value.kind != APG_V2_VALUE_LITERAL) {
            char msg[192];
            snprintf(
                msg, sizeof(msg), "node '%s' %s binding key '%s' must be a literal signal name", node_id ? node_id : "",
                bind_section_name(section), bindings[i].key ? bindings[i].key : ""
            );
            return set_error(err, UC_E_TYPE, msg);
        }
        int signal_index = find_signal_index(unit, bindings[i].value.text);
        if (signal_index < 0) {
            char msg[192];
            snprintf(
                msg, sizeof(msg), "node '%s' %s binding key '%s' references unknown signal '%s'",
                node_id ? node_id : "", bind_section_name(section), bindings[i].key ? bindings[i].key : "",
                bindings[i].value.text ? bindings[i].value.text : ""
            );
            return set_error(err, UC_E_MISSING, msg);
        }

        compiled[i].kind  = APG_BIND_SIGNAL;
        compiled[i].index = (size_t)signal_index;
    }

    *out_bindings = compiled;
    *out_len      = bindings_len;
    return UC_OK;
}

static uc_status compile_config_bindings(
    const apg_unit_v2_t         *unit,
    const char                  *node_id,
    const char                  *atom,
    const apg_unit_v2_binding_t *bindings,
    size_t                       bindings_len,
    uc_arena                    *arena,
    apg_v2_compiled_binding_t  **out_bindings,
    size_t                      *out_len,
    uc_error                    *err
) {
    *out_bindings = NULL;
    *out_len      = 0;
    uc_status status =
        validate_required_binding_keys(node_id, atom, APG_BIND_SECTION_CONFIG, bindings, bindings_len, err);
    if (status != UC_OK)
        return status;
    if (bindings_len == 0)
        return UC_OK;

    apg_v2_compiled_binding_t *compiled = uc_arena_alloc(arena, bindings_len * sizeof(*compiled), sizeof(void *));
    if (!compiled)
        return set_error(err, UC_E_OOM, "arena OOM");

    for (size_t i = 0; i < bindings_len; i++) {
        status = validate_binding_key(node_id, atom, APG_BIND_SECTION_CONFIG, bindings[i].key, err);
        if (status != UC_OK)
            return status;

        init_compiled_binding(&compiled[i], bindings[i].key);

        if (atom_config_key_is_float_matrix(atom, bindings[i].key)) {
            status = compile_float_matrix_binding(node_id, &bindings[i], arena, &compiled[i], err);
            if (status != UC_OK)
                return status;
            continue;
        }

        if (bindings[i].value.kind == APG_V2_VALUE_VARREF) {
            const char *param_name  = param_ref_name(bindings[i].value.text);
            int         param_index = find_param_index(unit, param_name);
            if (param_index < 0) {
                char msg[192];
                snprintf(
                    msg, sizeof(msg), "node '%s' config binding key '%s' references unknown parameter '%s'",
                    node_id ? node_id : "", bindings[i].key ? bindings[i].key : "",
                    bindings[i].value.text ? bindings[i].value.text : ""
                );
                return set_error(err, UC_E_MISSING, msg);
            }
            compiled[i].kind  = APG_BIND_PARAM;
            compiled[i].index = (size_t)param_index;
        } else {
            status = compile_scalar_literal(node_id, APG_BIND_SECTION_CONFIG, &compiled[i], &bindings[i], err);
            if (status != UC_OK)
                return status;
        }
    }

    *out_bindings = compiled;
    *out_len      = bindings_len;
    return UC_OK;
}

uc_status apg_v2_compile_unit(const apg_unit_v2_t *unit, uc_arena *arena, apg_v2_compiled_unit_t *out, uc_error *err) {
    if (!unit || !arena || !out || !err)
        return UC_E_TYPE;
    memset(out, 0, sizeof(*out));
    err->status = UC_OK;

    apg_v2_compiled_node_t *nodes    = uc_arena_alloc(arena, unit->nodes_len * sizeof(*nodes), sizeof(void *));
    uint32_t               *schedule = uc_arena_alloc(arena, unit->nodes_len * sizeof(*schedule), sizeof(uint32_t));
    int      *node_scheduled         = uc_arena_alloc(arena, unit->nodes_len * sizeof(*node_scheduled), sizeof(int));
    int      *signal_available = uc_arena_alloc(arena, unit->signals_len * sizeof(*signal_available), sizeof(int));
    uint32_t *signal_producers = uc_arena_alloc(arena, unit->signals_len * sizeof(*signal_producers), sizeof(uint32_t));
    if (((!nodes || !schedule || !node_scheduled) && unit->nodes_len > 0) ||
        ((!signal_available || !signal_producers) && unit->signals_len > 0))
        return set_error(err, UC_E_OOM, "arena OOM");
    for (size_t i = 0; i < unit->nodes_len; i++)
        node_scheduled[i] = 0;
    for (size_t i = 0; i < unit->signals_len; i++) {
        signal_available[i] = 0;
        signal_producers[i] = UINT32_MAX;
    }

    uc_status status = mark_input_port_signals(unit, signal_available, err);
    if (status != UC_OK)
        return status;

    atom_registry_init();
    for (size_t i = 0; i < unit->nodes_len; i++) {
        const apg_unit_v2_node_t    *src  = &unit->nodes[i];
        const atom_registry_entry_t *atom = atom_registry_find(src->atom);
        if (!atom) {
            char msg[160];
            snprintf(
                msg, sizeof(msg), "node '%s' references unknown atom '%s'", src->id ? src->id : "",
                src->atom ? src->atom : ""
            );
            return set_error(err, UC_E_MISSING, msg);
        }

        nodes[i].id = src->id;
        populate_atom_layout(&nodes[i], atom);

        status = compile_signal_bindings(
            unit, src->id, src->atom, APG_BIND_SECTION_IN, src->in, src->in_len, arena, &nodes[i].in, &nodes[i].in_len,
            err
        );
        if (status != UC_OK)
            return status;
        status = compile_signal_bindings(
            unit, src->id, src->atom, APG_BIND_SECTION_OUT, src->out, src->out_len, arena, &nodes[i].out,
            &nodes[i].out_len, err
        );
        if (status != UC_OK)
            return status;
        status = record_node_output_producers(&nodes[i], (uint32_t)i, signal_producers, err);
        if (status != UC_OK)
            return status;
        status = compile_config_bindings(
            unit, src->id, src->atom, src->config, src->config_len, arena, &nodes[i].config, &nodes[i].config_len, err
        );
        if (status != UC_OK)
            return status;
        status = validate_mix_matrix_shape(&nodes[i], err);
        if (status != UC_OK)
            return status;
    }

    status = build_schedule(nodes, unit->nodes_len, schedule, node_scheduled, signal_available, err);
    if (status != UC_OK)
        return status;

    status = validate_output_port_signals_produced(unit, signal_available, err);
    if (status != UC_OK)
        return status;

    out->unit                 = unit;
    out->nodes                = nodes;
    out->nodes_len            = unit->nodes_len;
    out->schedule             = schedule;
    out->schedule_len         = unit->nodes_len;
    out->signal_producers     = signal_producers;
    out->signal_producers_len = unit->signals_len;
    return build_instance_metadata(unit, nodes, unit->nodes_len, arena, out, err);
}
