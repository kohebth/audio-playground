#include <apgcore/runtime_image_v2.h>

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <atom/dsp_types.h>

static uc_status set_error(uc_error *err, uc_status status, const char *msg) {
    uc_loc loc = {0, 0};
    uc_error_set(err, status, loc, "%s", msg);
    return status;
}

static size_t atom_storage_size(size_t size) { return size > 0u ? size : 1u; }

static size_t runtime_storage_align(void) { return _Alignof(max_align_t); }

static bool align_up_size(size_t value, size_t alignment, size_t *out) {
    size_t mask = alignment - 1u;
    if (alignment == 0u || (alignment & mask) != 0u)
        return false;
    if (value > SIZE_MAX - mask)
        return false;
    *out = (value + mask) & ~mask;
    return true;
}

static uc_status reserve_storage(size_t size, size_t *cursor, size_t *out_offset, uc_error *err) {
    size_t aligned = 0u;
    if (!align_up_size(*cursor, runtime_storage_align(), &aligned))
        return set_error(err, UC_E_RANGE, "v2 runtime image atom storage alignment overflow");
    if (size > SIZE_MAX - aligned)
        return set_error(err, UC_E_RANGE, "v2 runtime image atom storage layout is too large");
    *out_offset = aligned;
    *cursor     = aligned + size;
    return UC_OK;
}

static bool parse_port_channel_count(const apg_unit_v2_port_t *port, size_t *out_count) {
    if (!port || !port->channels || !out_count || port->channels[0] == '\0')
        return false;
    char         *end   = NULL;
    unsigned long value = strtoul(port->channels, &end, 10);
    if (!end || *end != '\0' || value == 0ul)
        return false;
    *out_count = (size_t)value;
    return true;
}

static size_t audio_port_meter_count(const apg_unit_v2_port_t *ports, size_t ports_len) {
    size_t count = 0u;
    for (size_t i = 0; i < ports_len; i++) {
        if (!ports[i].type || strcmp(ports[i].type, "audio") != 0)
            continue;
        size_t channels = 0u;
        if (parse_port_channel_count(&ports[i], &channels))
            count += channels;
    }
    return count;
}

static float parse_param_default(const apg_unit_v2_param_t *param) {
    if (!param || !param->default_value)
        return 0.0f;
    if (param->type && strcmp(param->type, "bool") == 0)
        return strcmp(param->default_value, "true") == 0 ? 1.0f : 0.0f;
    return strtof(param->default_value, NULL);
}

static int param_index_by_name(const apg_unit_v2_t *unit, const char *name) {
    if (!unit || !name)
        return -1;
    for (size_t i = 0; i < unit->params_len; i++) {
        if (unit->params[i].name && strcmp(unit->params[i].name, name) == 0)
            return (int)i;
    }
    return -1;
}

static size_t control_port_count(const apg_unit_v2_port_t *ports, size_t ports_len) {
    size_t count = 0u;
    for (size_t i = 0; i < ports_len; i++) {
        if (ports[i].type && strcmp(ports[i].type, "control") == 0)
            count++;
    }
    return count;
}

static size_t signal_array_pointer_slots(const apg_v2_compiled_binding_t *bindings, size_t bindings_len) {
    size_t slots = 0u;
    for (size_t i = 0; i < bindings_len; i++) {
        if (bindings[i].kind == APG_BIND_SIGNAL_ARRAY)
            slots += bindings[i].indices_len;
    }
    return slots;
}

static const atom_field_desc_t delay_tap_feedback_in_fields[] = {
    {      "buffer", FIELD_SIGNAL, offsetof(delay_tap_feedback_in_t,       buffer)},
    {"tap_position",    FIELD_INT, offsetof(delay_tap_feedback_in_t, tap_position)},
};

static const atom_field_desc_t delay_tap_feedforward_in_fields[] = {
    {      "buffer", FIELD_SIGNAL, offsetof(delay_tap_feedforward_in_t,       buffer)},
    {"tap_position",    FIELD_INT, offsetof(delay_tap_feedforward_in_t, tap_position)},
};

static const atom_field_desc_t *
find_field_in_list(const atom_field_desc_t *fields, size_t fields_len, const char *key) {
    if (!fields || !key)
        return NULL;
    for (size_t i = 0; i < fields_len; i++) {
        if (fields[i].name && strcmp(fields[i].name, key) == 0)
            return &fields[i];
    }
    return NULL;
}

static const atom_field_desc_t *find_input_field(const atom_registry_entry_t *atom, const char *key) {
    if (!atom || !atom->name)
        return NULL;
    if (strcmp(atom->name, "delay_tap_feedback") == 0)
        return find_field_in_list(
            delay_tap_feedback_in_fields,
            sizeof(delay_tap_feedback_in_fields) / sizeof(delay_tap_feedback_in_fields[0]), key
        );
    if (strcmp(atom->name, "delay_tap_feedforward") == 0)
        return find_field_in_list(
            delay_tap_feedforward_in_fields,
            sizeof(delay_tap_feedforward_in_fields) / sizeof(delay_tap_feedforward_in_fields[0]), key
        );
    return NULL;
}

static const atom_field_desc_t *find_config_field(const atom_registry_entry_t *atom, const char *key) {
    if (!atom || !key)
        return NULL;
    for (int i = 0; i < atom->n_config_fields; i++) {
        if (atom->config_fields[i].name && strcmp(atom->config_fields[i].name, key) == 0)
            return &atom->config_fields[i];
    }
    return NULL;
}

static bool scalar_refresh_field(const atom_field_desc_t *field) {
    return field && (field->type == FIELD_INT || field->type == FIELD_FLOAT);
}

static size_t count_config_refreshes(const apg_v2_compiled_node_t *node) {
    size_t count = 0u;
    for (size_t i = 0; node && i < node->config_len; i++) {
        if (node->config[i].kind == APG_BIND_FLOAT_MATRIX)
            continue;
        count++;
    }
    return count;
}

static size_t count_input_refreshes(const apg_v2_compiled_node_t *node) {
    size_t count = 0u;
    for (size_t i = 0; node && i < node->in_len; i++) {
        const atom_field_desc_t *field = find_input_field(node->atom, node->in[i].key);
        if (scalar_refresh_field(field))
            count++;
    }
    return count;
}

static uc_status fill_scalar_refreshes(
    uc_arena                         *arena,
    const apg_v2_compiled_node_t     *node,
    apg_v2_runtime_scalar_refresh_t **out_items,
    size_t                           *out_len,
    bool                              config,
    uc_error                         *err
) {
    size_t len = config ? count_config_refreshes(node) : count_input_refreshes(node);
    *out_items = NULL;
    *out_len   = len;
    if (len == 0u)
        return UC_OK;

    apg_v2_runtime_scalar_refresh_t *items = uc_arena_alloc(arena, len * sizeof(*items), sizeof(void *));
    if (!items)
        return set_error(err, UC_E_OOM, "v2 runtime image scalar refresh allocation failed");

    size_t                           item_index  = 0u;
    size_t                           binding_len = config ? node->config_len : node->in_len;
    const apg_v2_compiled_binding_t *bindings    = config ? node->config : node->in;
    for (size_t i = 0; i < binding_len; i++) {
        if (config && bindings[i].kind == APG_BIND_FLOAT_MATRIX)
            continue;
        const atom_field_desc_t *field =
            config ? find_config_field(node->atom, bindings[i].key) : find_input_field(node->atom, bindings[i].key);
        if (!field && !config)
            continue;
        if (!config && !scalar_refresh_field(field))
            continue;
        if (!field)
            return set_error(err, UC_E_MISSING, "v2 runtime image config refresh metadata is missing");
        if (!scalar_refresh_field(field))
            return set_error(err, UC_E_TYPE, "v2 runtime image scalar refresh field type is unsupported");
        items[item_index].binding        = &bindings[i];
        items[item_index].node_id        = node->id;
        items[item_index].atom_name      = node->atom ? node->atom->name : NULL;
        items[item_index].binding_key    = bindings[i].key;
        items[item_index].storage_offset = field->offset;
        items[item_index].field_type     = field->type;
        items[item_index].config         = config;
        item_index++;
    }

    *out_items = items;
    return UC_OK;
}

static uc_status fill_control_targets(uc_arena *arena, apg_v2_runtime_image_t *out, uc_error *err) {
    const apg_unit_v2_t *unit = out->plan->unit;
    out->control_targets_len  = control_port_count(unit->input_ports, unit->input_ports_len);
    if (out->control_targets_len == 0u)
        return UC_OK;

    out->control_targets =
        uc_arena_alloc(arena, out->control_targets_len * sizeof(*out->control_targets), sizeof(void *));
    if (!out->control_targets)
        return set_error(err, UC_E_OOM, "v2 runtime image control target allocation failed");

    size_t target_index = 0u;
    for (size_t i = 0; i < unit->input_ports_len; i++) {
        const apg_unit_v2_port_t *port = &unit->input_ports[i];
        if (!port->type || strcmp(port->type, "control") != 0)
            continue;
        if (port->target_kind && strcmp(port->target_kind, "param") != 0)
            return set_error(err, UC_E_TYPE, "v2 runtime image only supports param control targets");
        const char *target =
            port->target_name ? port->target_name : (port->target_param ? port->target_param : port->name);
        int index = param_index_by_name(unit, target);
        if (index < 0)
            return set_error(err, UC_E_MISSING, "v2 runtime image control target param is missing");
        out->control_targets[target_index].port_name   = port->name;
        out->control_targets[target_index].param_name  = target;
        out->control_targets[target_index].param_index = (size_t)index;
        target_index++;
    }
    return UC_OK;
}

static uc_status fill_node_layouts(uc_arena *arena, apg_v2_runtime_image_t *out, uc_error *err) {
    out->nodes_len = out->plan->nodes_len;
    if (out->nodes_len == 0u)
        return UC_OK;

    out->node_layouts = uc_arena_alloc(arena, out->nodes_len * sizeof(*out->node_layouts), sizeof(void *));
    if (!out->node_layouts)
        return set_error(err, UC_E_OOM, "v2 runtime image node layout allocation failed");

    size_t atom_storage_cursor = 0u;
    for (size_t node_index = 0; node_index < out->nodes_len; node_index++) {
        const atom_registry_entry_t  *atom   = out->plan->nodes[node_index].atom;
        apg_v2_runtime_node_layout_t *layout = &out->node_layouts[node_index];
        if (!atom)
            return set_error(err, UC_E_MISSING, "v2 runtime image node is missing atom metadata");

        layout->out_size    = atom_storage_size(atom->out_size);
        layout->in_size     = atom_storage_size(atom->in_size);
        layout->config_size = atom_storage_size(atom->config_size);
        layout->state_size  = atom_storage_size(atom->state_size);
        uc_status status    = reserve_storage(layout->out_size, &atom_storage_cursor, &layout->out_offset, err);
        if (status != UC_OK)
            return status;
        status = reserve_storage(layout->in_size, &atom_storage_cursor, &layout->in_offset, err);
        if (status != UC_OK)
            return status;
        status = reserve_storage(layout->config_size, &atom_storage_cursor, &layout->config_offset, err);
        if (status != UC_OK)
            return status;
        status = reserve_storage(layout->state_size, &atom_storage_cursor, &layout->state_offset, err);
        if (status != UC_OK)
            return status;
        layout->signal_array_pointer_slots =
            signal_array_pointer_slots(out->plan->nodes[node_index].in, out->plan->nodes[node_index].in_len) +
            signal_array_pointer_slots(out->plan->nodes[node_index].out, out->plan->nodes[node_index].out_len);

        for (int field_index = 0; field_index < atom->n_state_fields; field_index++) {
            if (atom->state_fields[field_index].type == FIELD_BUFFER)
                layout->state_buffers_len++;
        }
        if (layout->state_buffers_len > 0u) {
            layout->state_buffer_samples_by_index = uc_arena_alloc(
                arena, layout->state_buffers_len * sizeof(*layout->state_buffer_samples_by_index), sizeof(size_t)
            );
            if (!layout->state_buffer_samples_by_index)
                return set_error(err, UC_E_OOM, "v2 runtime image state buffer layout allocation failed");
        }

        size_t buffer_index = 0u;
        for (int field_index = 0; field_index < atom->n_state_fields; field_index++) {
            if (atom->state_fields[field_index].type != FIELD_BUFFER)
                continue;
            layout->state_buffer_samples_by_index[buffer_index++] = atom->state_fields[field_index].buffer_samples;
            layout->state_buffer_samples += atom->state_fields[field_index].buffer_samples;
        }
        out->state_buffers_len += layout->state_buffers_len;
        out->state_buffer_samples += layout->state_buffer_samples;

        status = fill_scalar_refreshes(
            arena, &out->plan->nodes[node_index], &layout->config_refreshes, &layout->config_refreshes_len, true, err
        );
        if (status != UC_OK)
            return status;
        status = fill_scalar_refreshes(
            arena, &out->plan->nodes[node_index], &layout->input_refreshes, &layout->input_refreshes_len, false, err
        );
        if (status != UC_OK)
            return status;
    }
    out->atom_storage_bytes = atom_storage_cursor;
    return UC_OK;
}

uc_status apg_v2_runtime_image_build(
    const apg_v2_compiled_unit_t *plan,
    uint32_t                      frame_capacity,
    float                         sample_rate,
    uc_arena                     *arena,
    apg_v2_runtime_image_t       *out,
    uc_error                     *err
) {
    if (!plan || !plan->unit || !arena || !out || !err)
        return UC_E_TYPE;
    memset(out, 0, sizeof(*out));
    err->status = UC_OK;
    if (frame_capacity == 0u)
        return set_error(err, UC_E_RANGE, "v2 runtime image frame capacity must be greater than zero");
    if (plan->unit->signals_len > 0u && plan->unit->signals_len > SIZE_MAX / (size_t)frame_capacity)
        return set_error(err, UC_E_RANGE, "v2 runtime image signal layout is too large");

    out->plan              = plan;
    out->frame_capacity    = frame_capacity;
    out->sample_rate       = sample_rate > 0.0f ? sample_rate : 48000.0f;
    out->signals_len       = plan->unit->signals_len;
    out->signal_samples    = plan->unit->signals_len * (size_t)frame_capacity;
    out->params_len        = plan->unit->params_len;
    out->input_meters_len  = audio_port_meter_count(plan->unit->input_ports, plan->unit->input_ports_len);
    out->output_meters_len = audio_port_meter_count(plan->unit->output_ports, plan->unit->output_ports_len);
    out->schedule_len      = plan->schedule_len;

    uc_status status = fill_node_layouts(arena, out, err);
    if (status != UC_OK)
        return status;

    status = fill_control_targets(arena, out, err);
    if (status != UC_OK)
        return status;

    if (out->params_len == 0u)
        return UC_OK;
    out->param_defaults = uc_arena_alloc(arena, out->params_len * sizeof(*out->param_defaults), sizeof(float));
    if (!out->param_defaults)
        return set_error(err, UC_E_OOM, "v2 runtime image param defaults allocation failed");
    for (size_t i = 0; i < out->params_len; i++)
        out->param_defaults[i] = parse_param_default(&plan->unit->params[i]);
    return UC_OK;
}
