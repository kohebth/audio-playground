#include <apgcore/runtime_image_v2.h>

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

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
    size_t                   fields_len = 0u;
    const atom_field_desc_t *fields     = atom_registry_in_fields(atom, &fields_len);
    return find_field_in_list(fields, fields_len, key);
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

static size_t count_signal_bindings(const apg_v2_compiled_binding_t *bindings, size_t bindings_len) {
    size_t count = 0u;
    for (size_t i = 0; i < bindings_len; i++) {
        if (bindings[i].kind == APG_BIND_SIGNAL || bindings[i].kind == APG_BIND_SIGNAL_ARRAY)
            count++;
    }
    return count;
}

static const apg_v2_compiled_binding_t *
find_compiled_binding(const apg_v2_compiled_binding_t *bindings, size_t bindings_len, const char *key) {
    for (size_t i = 0; i < bindings_len; i++) {
        if (bindings[i].key && key && strcmp(bindings[i].key, key) == 0)
            return &bindings[i];
    }
    return NULL;
}

static bool is_mix_matrix_node(const apg_v2_compiled_node_t *node) {
    return node && node->atom && node->atom->name && strcmp(node->atom->name, "mix_matrix") == 0;
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

static uc_status fill_signal_bindings(
    const apg_v2_runtime_image_t    *image,
    const apg_v2_compiled_node_t    *node,
    const apg_v2_compiled_binding_t *bindings,
    size_t                           bindings_len,
    bool                             is_input,
    size_t                          *array_cursor,
    apg_v2_runtime_signal_binding_t *items,
    size_t                           items_cap,
    size_t                          *items_len,
    apg_v2_runtime_node_layout_t    *layout,
    size_t                           node_array_base,
    uc_error                        *err
) {
    size_t len = 0u;
    for (size_t i = 0; i < bindings_len; i++) {
        if (bindings[i].kind == APG_BIND_SIGNAL || bindings[i].kind == APG_BIND_SIGNAL_ARRAY)
            len++;
    }

    if (len == 0u)
        return UC_OK;
    if (!items || !items_len)
        return set_error(err, UC_E_MISSING, "v2 runtime image signal binding output buffer is missing");
    if (*items_len > SIZE_MAX - len || *items_len + len > items_cap)
        return set_error(err, UC_E_RANGE, "v2 runtime image signal binding layout is inconsistent");
    if (!array_cursor)
        return set_error(err, UC_E_RANGE, "v2 runtime image signal array cursor is missing");
    if (*array_cursor < node_array_base)
        return set_error(err, UC_E_RANGE, "v2 runtime image signal binding layout is inconsistent");

    size_t local_array_cursor = 0u;
    size_t item_index         = 0u;
    for (size_t i = 0; i < bindings_len; i++) {
        const apg_v2_compiled_binding_t *binding = &bindings[i];
        if (binding->kind != APG_BIND_SIGNAL && binding->kind != APG_BIND_SIGNAL_ARRAY)
            continue;

        apg_v2_runtime_signal_binding_t item = {0};
        item.binding                         = binding;
        item.is_input                        = is_input;
        item.storage_offset                  = i * sizeof(float *);
        item.signal_array_len                = 0u;

        if (is_input) {
            const atom_field_desc_t *field = find_input_field(node->atom, binding->key);
            if (field) {
                if (field->type != FIELD_SIGNAL) {
                    if (binding->kind == APG_BIND_SIGNAL)
                        return set_error(err, UC_E_TYPE, "v2 runtime image input binding field is not a signal");
                    return set_error(
                        err, UC_E_TYPE, "v2 runtime image input binding field does not support signal arrays"
                    );
                }
                item.storage_offset = field->offset;
            }
            if (binding->kind == APG_BIND_SIGNAL) {
                if (binding->index >= image->signals_len)
                    return set_error(
                        err, UC_E_MISSING, "v2 runtime image input binding references invalid signal index"
                    );
                item.signal_index = binding->index;
            } else {
                item.is_signal_array  = true;
                item.signal_index     = SIZE_MAX;
                item.signal_array_len = binding->indices_len;
                if (binding->indices_len > SIZE_MAX - local_array_cursor)
                    return set_error(err, UC_E_RANGE, "v2 runtime image signal binding layout is too large");
                item.signal_array_offset = *array_cursor - node_array_base + local_array_cursor;
                if (binding->indices_len == 0u)
                    return set_error(err, UC_E_RANGE, "v2 runtime image input binding has empty signal array");
                for (size_t j = 0; j < binding->indices_len; j++) {
                    if (binding->indices[j] >= image->signals_len)
                        return set_error(
                            err, UC_E_MISSING, "v2 runtime image input binding references invalid signal index"
                        );
                }
                local_array_cursor += binding->indices_len;
            }
        } else {
            if (binding->kind == APG_BIND_SIGNAL) {
                if (binding->index >= image->signals_len)
                    return set_error(
                        err, UC_E_MISSING, "v2 runtime image output binding references invalid signal index"
                    );
                item.signal_index = binding->index;
            } else {
                item.is_signal_array  = true;
                item.signal_index     = SIZE_MAX;
                item.signal_array_len = binding->indices_len;
                if (binding->indices_len > SIZE_MAX - local_array_cursor)
                    return set_error(err, UC_E_RANGE, "v2 runtime image signal binding layout is too large");
                item.signal_array_offset = *array_cursor - node_array_base + local_array_cursor;
                if (binding->indices_len == 0u)
                    return set_error(err, UC_E_RANGE, "v2 runtime image output binding has empty signal array");
                for (size_t j = 0; j < binding->indices_len; j++) {
                    if (binding->indices[j] >= image->signals_len)
                        return set_error(
                            err, UC_E_MISSING, "v2 runtime image output binding references invalid signal index"
                        );
                }
                local_array_cursor += binding->indices_len;
            }
        }

        items[*items_len + item_index++] = item;
    }

    if (local_array_cursor > layout->signal_array_pointer_slots)
        return set_error(err, UC_E_RANGE, "v2 runtime image signal binding layout is inconsistent");
    if (item_index != len)
        return set_error(err, UC_E_RANGE, "v2 runtime image signal binding layout is inconsistent");
    if (local_array_cursor > SIZE_MAX - *array_cursor)
        return set_error(err, UC_E_RANGE, "v2 runtime image signal array cursor overflow");
    *array_cursor += local_array_cursor;
    *items_len += item_index;
    return UC_OK;
}

static uc_status fill_mix_matrix_layout(
    uc_arena *arena, const apg_v2_compiled_node_t *node, apg_v2_runtime_node_layout_t *layout, uc_error *err
) {
    if (!is_mix_matrix_node(node))
        return UC_OK;

    const apg_v2_compiled_binding_t *matrix = find_compiled_binding(node->config, node->config_len, "coefficients");
    if (!matrix || matrix->kind != APG_BIND_FLOAT_MATRIX)
        return set_error(err, UC_E_TYPE, "v2 runtime image mix_matrix requires coefficient matrix binding");
    if (matrix->rows == 0u || matrix->cols == 0u)
        return set_error(err, UC_E_RANGE, "v2 runtime image mix_matrix requires non-empty coefficient matrix");
    if (matrix->numbers == NULL)
        return set_error(err, UC_E_MISSING, "v2 runtime image mix_matrix coefficient data is missing");

    if (matrix->rows > SIZE_MAX / matrix->cols)
        return set_error(err, UC_E_RANGE, "v2 runtime image mix_matrix matrix size overflow");

    size_t coefficient_count            = matrix->rows * matrix->cols;
    layout->mix_matrix_coefficients_len = coefficient_count;
    layout->mix_matrix_num_out          = matrix->rows;
    layout->mix_matrix_num_in           = matrix->cols;

    layout->mix_matrix_coefficients =
        uc_arena_alloc(arena, coefficient_count * sizeof(*layout->mix_matrix_coefficients), sizeof(float));
    if (!layout->mix_matrix_coefficients)
        return set_error(err, UC_E_OOM, "v2 runtime image mix_matrix coefficient allocation failed");
    memcpy(layout->mix_matrix_coefficients, matrix->numbers, coefficient_count * sizeof(float));

    layout->mix_matrix_row_pointers =
        uc_arena_alloc(arena, matrix->rows * sizeof(*layout->mix_matrix_row_pointers), sizeof(float *));
    if (!layout->mix_matrix_row_pointers)
        return set_error(err, UC_E_OOM, "v2 runtime image mix_matrix row pointer allocation failed");

    for (size_t row = 0u; row < matrix->rows; row++)
        layout->mix_matrix_row_pointers[row] = &layout->mix_matrix_coefficients[row * matrix->cols];

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
    size_t state_buffer_cursor = 0u;
    size_t signal_array_cursor = 0u;
    for (size_t node_index = 0; node_index < out->nodes_len; node_index++) {
        const atom_registry_entry_t  *atom   = out->plan->nodes[node_index].atom;
        apg_v2_runtime_node_layout_t *layout = &out->node_layouts[node_index];
        if (!atom)
            return set_error(err, UC_E_MISSING, "v2 runtime image node is missing atom metadata");

        memset(layout, 0, sizeof(*layout));

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
        layout->signal_array_pool_offset = signal_array_cursor;

        for (int field_index = 0; field_index < atom->n_state_fields; field_index++) {
            if (atom->state_fields[field_index].type == FIELD_BUFFER)
                layout->state_buffers_len++;
        }
        if (layout->state_buffers_len > 0u) {
            layout->state_buffer_samples_by_index = uc_arena_alloc(
                arena, layout->state_buffers_len * sizeof(*layout->state_buffer_samples_by_index), sizeof(size_t)
            );
            layout->state_buffer_sample_offsets_by_index = uc_arena_alloc(
                arena, layout->state_buffers_len * sizeof(*layout->state_buffer_sample_offsets_by_index), sizeof(size_t)
            );
            if (!layout->state_buffer_samples_by_index || !layout->state_buffer_sample_offsets_by_index)
                return set_error(err, UC_E_OOM, "v2 runtime image state buffer layout allocation failed");
        }

        size_t buffer_index = 0u;
        for (int field_index = 0; field_index < atom->n_state_fields; field_index++) {
            if (atom->state_fields[field_index].type != FIELD_BUFFER)
                continue;
            layout->state_buffer_samples_by_index[buffer_index++] = atom->state_fields[field_index].buffer_samples;
            layout->state_buffer_sample_offsets_by_index[buffer_index - 1u] = state_buffer_cursor;
            if (atom->state_fields[field_index].buffer_samples > SIZE_MAX - state_buffer_cursor)
                return set_error(err, UC_E_RANGE, "v2 runtime image state buffer layout is too large");
            state_buffer_cursor += atom->state_fields[field_index].buffer_samples;
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

        size_t signal_bindings_len =
            count_signal_bindings(out->plan->nodes[node_index].out, out->plan->nodes[node_index].out_len) +
            count_signal_bindings(out->plan->nodes[node_index].in, out->plan->nodes[node_index].in_len);
        layout->signal_bindings_len = signal_bindings_len;
        if (signal_bindings_len > 0u) {
            layout->signal_bindings =
                uc_arena_alloc(arena, signal_bindings_len * sizeof(*layout->signal_bindings), sizeof(void *));
            if (!layout->signal_bindings)
                return set_error(err, UC_E_OOM, "v2 runtime image signal binding allocation failed");
        }

        size_t signal_binding_index = 0u;

        status = fill_signal_bindings(
            out, &out->plan->nodes[node_index], out->plan->nodes[node_index].out, out->plan->nodes[node_index].out_len,
            false, &signal_array_cursor, layout->signal_bindings, layout->signal_bindings_len, &signal_binding_index,
            layout, layout->signal_array_pool_offset, err
        );
        if (status != UC_OK)
            return status;

        status = fill_signal_bindings(
            out, &out->plan->nodes[node_index], out->plan->nodes[node_index].in, out->plan->nodes[node_index].in_len,
            true, &signal_array_cursor, layout->signal_bindings, layout->signal_bindings_len, &signal_binding_index,
            layout, layout->signal_array_pool_offset, err
        );
        if (status != UC_OK)
            return status;
        if (signal_binding_index != layout->signal_bindings_len)
            return set_error(err, UC_E_RANGE, "v2 runtime image signal binding layout is inconsistent");

        if (signal_array_cursor - layout->signal_array_pool_offset > layout->signal_array_pointer_slots)
            return set_error(err, UC_E_RANGE, "v2 runtime image signal binding layout is inconsistent");

        status = fill_mix_matrix_layout(arena, &out->plan->nodes[node_index], layout, err);
        if (status != UC_OK)
            return status;
    }
    out->signal_array_pointer_slots = signal_array_cursor;
    out->state_buffer_samples       = state_buffer_cursor;
    out->atom_storage_bytes         = atom_storage_cursor;
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

uc_status apg_v2_runtime_image_build_with_growth(
    const apg_v2_compiled_unit_t *plan,
    uint32_t                      frame_capacity,
    float                         sample_rate,
    uc_arena                     *out_arena,
    apg_v2_runtime_image_t       *out_image,
    uc_error                     *err
) {
    if (!plan || !out_arena || !out_image || !err)
        return UC_E_TYPE;

    *out_arena = (uc_arena){0};
    memset(out_image, 0, sizeof(*out_image));

    size_t image_arena_size = 4096u;
    while (image_arena_size > 0u && image_arena_size <= (SIZE_MAX >> 1)) {
        uc_arena image_arena = {0};
        if (uc_arena_init(&image_arena, image_arena_size) != 0) {
            return set_error(err, UC_E_OOM, "v2 runtime image arena allocation failed");
        }

        uc_status status = apg_v2_runtime_image_build(plan, frame_capacity, sample_rate, &image_arena, out_image, err);
        if (status == UC_OK) {
            *out_arena = image_arena;
            return UC_OK;
        }

        uc_arena_free(&image_arena);
        if (status != UC_E_OOM)
            return status;
        image_arena_size *= 2u;
    }

    return set_error(err, UC_E_OOM, "v2 runtime image arena growth overflow");
}
