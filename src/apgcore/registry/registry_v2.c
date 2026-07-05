#include <apgcore/registry/registry_builder_v2.h>

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uc_status set_error(uc_error *err, uc_status status, const char *msg) {
    uc_loc loc = {0, 0};
    uc_error_set(err, status, loc, "%s", msg);
    return status;
}

static size_t atom_storage_size(size_t size) { return size > 0u ? size : 1u; }

static size_t runtime_storage_align(void) { return _Alignof(max_align_t); }

static bool parse_port_channel_count(const apg_unit_v2_port_t *port, size_t *out_count);

static int signal_index_by_name(const apg_unit_v2_t *unit, const char *name) {
    if (!unit || !name)
        return -1;
    for (size_t i = 0; i < unit->signals_len; i++) {
        if (unit->signals[i] && strcmp(unit->signals[i], name) == 0)
            return (int)i;
    }
    return -1;
}

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
        return set_error(err, UC_E_RANGE, "v2 registry atom storage alignment overflow");
    if (size > SIZE_MAX - aligned)
        return set_error(err, UC_E_RANGE, "v2 registry atom storage layout is too large");
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

static size_t audio_port_count(const apg_unit_v2_port_t *ports, size_t ports_len) {
    size_t count = 0u;
    for (size_t i = 0; i < ports_len; i++) {
        if (ports[i].type && strcmp(ports[i].type, "audio") == 0)
            count++;
    }
    return count;
}

static const char *audio_port_channel_signal_name(const apg_unit_v2_port_t *port, size_t channel_index) {
    if (!port)
        return NULL;
    if (port->signals_len > 0u)
        return channel_index < port->signals_len ? port->signals[channel_index] : NULL;
    return channel_index == 0u ? port->name : NULL;
}

static uc_status fill_audio_port_map(
    uc_arena                      *arena,
    const apg_unit_v2_t           *unit,
    const apg_unit_v2_port_t      *ports,
    size_t                         ports_len,
    apg_v2_registry_audio_port_t **out_ports,
    size_t                        *out_ports_len,
    uc_error                      *err
) {
    size_t port_count = audio_port_count(ports, ports_len);
    *out_ports        = NULL;
    *out_ports_len    = 0u;
    if (port_count == 0u)
        return UC_OK;

    apg_v2_registry_audio_port_t *items = uc_arena_alloc(arena, port_count * sizeof(*items), sizeof(void *));
    if (!items)
        return set_error(err, UC_E_OOM, "v2 registry audio port map allocation failed");

    size_t port_index  = 0u;
    size_t meter_index = 0u;
    for (size_t i = 0; i < ports_len; i++) {
        const apg_unit_v2_port_t *port = &ports[i];
        if (!port->type || strcmp(port->type, "audio") != 0)
            continue;

        size_t channels = 0u;
        if (!parse_port_channel_count(port, &channels))
            return set_error(err, UC_E_RANGE, "v2 registry audio port channel count is invalid");

        size_t *indices = uc_arena_alloc(arena, channels * sizeof(*indices), sizeof(size_t));
        if (!indices)
            return set_error(err, UC_E_OOM, "v2 registry audio port signal map allocation failed");

        for (size_t ch = 0; ch < channels; ch++) {
            int signal_index = signal_index_by_name(unit, audio_port_channel_signal_name(port, ch));
            if (signal_index < 0)
                return set_error(err, UC_E_MISSING, "v2 registry audio port signal is missing");
            indices[ch] = (size_t)signal_index;
        }

        items[port_index++] = (apg_v2_registry_audio_port_t){
            .port_name      = port->name,
            .channel_count  = channels,
            .meter_index    = meter_index,
            .signal_indices = indices,
        };
        meter_index += channels;
    }

    *out_ports     = items;
    *out_ports_len = port_index;
    return UC_OK;
}

static float parse_param_default(const apg_unit_v2_param_t *param) {
    if (!param || !param->default_value)
        return 0.0f;
    if (param->type && strcmp(param->type, "bool") == 0)
        return strcmp(param->default_value, "true") == 0 ? 1.0f : 0.0f;
    return strtof(param->default_value, NULL);
}

static uint32_t param_smoothing_frames(const apg_unit_v2_param_t *param, float sample_rate) {
    if (!param || !param->smoothing_ms)
        return 0u;
    float smoothing_ms = strtof(param->smoothing_ms, NULL);
    if (smoothing_ms <= 0.0f)
        return 0u;
    double frames = ((double)smoothing_ms * (double)sample_rate) / 1000.0;
    if (frames >= (double)UINT32_MAX)
        return UINT32_MAX;
    uint32_t rounded = (uint32_t)(frames + 0.999999);
    return rounded > 0u ? rounded : 1u;
}

static uc_status
fill_bypass_metadata(uc_arena *arena, const apg_v2_compiled_unit_t *plan, apg_v2_registry_t *out, uc_error *err) {
    if (!arena || !plan || !out)
        return UC_OK;

    if (plan->nodes_len == 0u || plan->instances_len == 0u || !plan->instances)
        return UC_OK;

    apg_v2_registry_bypass_entry_t *entries =
        uc_arena_alloc(arena, plan->instances_len * sizeof(*entries), sizeof(void *));
    if (!entries)
        return set_error(err, UC_E_OOM, "v2 registry bypass metadata allocation failed");

    size_t bypass_count = 0u;
    for (size_t i = 0; i < plan->instances_len; i++) {
        const apg_v2_compiled_instance_t *instance = &plan->instances[i];
        if (!instance->bypassable)
            continue;

        if (bypass_count >= plan->instances_len)
            return set_error(err, UC_E_RANGE, "v2 registry bypass metadata overflow");
        entries[bypass_count++] = (apg_v2_registry_bypass_entry_t){
            .instance_id     = instance->id,
            .instance_id_len = instance->id_len,
            .input_index     = instance->input_signal_index,
            .output_index    = instance->output_signal_index,
        };
    }

    out->bypassed_instances_len = bypass_count;
    if (bypass_count == 0u)
        return UC_OK;

    out->bypass_instances = uc_arena_alloc(arena, bypass_count * sizeof(*out->bypass_instances), sizeof(void *));
    if (!out->bypass_instances)
        return set_error(err, UC_E_OOM, "v2 registry bypass entry allocation failed");
    memcpy(out->bypass_instances, entries, bypass_count * sizeof(*out->bypass_instances));

    out->bypass_index_by_node =
        uc_arena_alloc(arena, out->nodes_len * sizeof(*out->bypass_index_by_node), sizeof(void *));
    if (!out->bypass_index_by_node)
        return set_error(err, UC_E_OOM, "v2 registry bypass index metadata allocation failed");

    for (size_t i = 0; i < out->nodes_len; i++)
        out->bypass_index_by_node[i] = (size_t)-1u;

    for (size_t i = 0; i < bypass_count; i++) {
        const char *instance_id  = out->bypass_instances[i].instance_id;
        size_t      instance_len = out->bypass_instances[i].instance_id_len;
        if (!instance_id || instance_len == 0u)
            continue;

        for (size_t node_index = 0; node_index < out->nodes_len; node_index++) {
            if (!plan->instance_index_by_node || node_index >= plan->instance_index_by_node_len)
                continue;
            size_t instance_index = plan->instance_index_by_node[node_index];
            if (instance_index < plan->instances_len && plan->instances[instance_index].id_len == instance_len &&
                strncmp(plan->instances[instance_index].id, instance_id, instance_len) == 0)
                out->bypass_index_by_node[node_index] = i;
        }
    }

    return UC_OK;
}

static uc_status
fill_project_mute_output_indices(uc_arena *arena, const apg_unit_v2_t *unit, apg_v2_registry_t *out, uc_error *err) {
    if (!arena || !unit || !out)
        return UC_OK;

    size_t count = 0u;
    for (size_t i = 0; i < unit->output_ports_len; i++) {
        const apg_unit_v2_port_t *port = &unit->output_ports[i];
        if (!port || !port->type || strcmp(port->type, "audio") != 0)
            continue;
        size_t channels = 0u;
        if (!parse_port_channel_count(port, &channels))
            continue;
        count += channels;
    }
    if (count == 0u)
        return UC_OK;

    out->project_mute_output_indices =
        uc_arena_alloc(arena, count * sizeof(*out->project_mute_output_indices), sizeof(size_t));
    if (!out->project_mute_output_indices)
        return set_error(err, UC_E_OOM, "v2 registry project mute output index allocation failed");

    size_t filled = 0u;
    for (size_t i = 0; i < unit->output_ports_len; i++) {
        const apg_unit_v2_port_t *port = &unit->output_ports[i];
        if (!port || !port->type || strcmp(port->type, "audio") != 0)
            continue;
        size_t channels = 0u;
        if (!parse_port_channel_count(port, &channels))
            continue;
        for (size_t ch = 0; ch < channels; ch++) {
            const char *signal_name = NULL;
            if (port->signals_len > 0u) {
                if (ch < port->signals_len)
                    signal_name = port->signals[ch];
            } else {
                signal_name = port->name;
            }

            if (!signal_name)
                continue;
            int signal_index = signal_index_by_name(unit, signal_name);
            if (signal_index < 0)
                continue;
            out->project_mute_output_indices[filled++] = (size_t)signal_index;
        }
    }

    out->project_mute_output_indices_len = filled;
    return UC_OK;
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

static uc_status fill_signal_names(uc_arena *arena, const apg_unit_v2_t *unit, apg_v2_registry_t *out, uc_error *err) {
    if (out->signals_len == 0u)
        return UC_OK;
    out->signal_names = uc_arena_alloc(arena, out->signals_len * sizeof(*out->signal_names), sizeof(void *));
    if (!out->signal_names)
        return set_error(err, UC_E_OOM, "v2 registry signal name allocation failed");
    for (size_t i = 0; i < out->signals_len; i++)
        out->signal_names[i] = unit->signals[i];
    return UC_OK;
}

static uc_status fill_param_names(uc_arena *arena, const apg_unit_v2_t *unit, apg_v2_registry_t *out, uc_error *err) {
    if (out->params_len == 0u)
        return UC_OK;
    out->param_names = uc_arena_alloc(arena, out->params_len * sizeof(*out->param_names), sizeof(void *));
    if (!out->param_names)
        return set_error(err, UC_E_OOM, "v2 registry param name allocation failed");
    for (size_t i = 0; i < out->params_len; i++)
        out->param_names[i] = unit->params[i].name;
    return UC_OK;
}

static uc_status
fill_schedule(uc_arena *arena, const apg_v2_compiled_unit_t *plan, apg_v2_registry_t *out, uc_error *err) {
    if (out->schedule_len == 0u)
        return UC_OK;
    uint32_t *schedule = uc_arena_alloc(arena, out->schedule_len * sizeof(*schedule), sizeof(*schedule));
    if (!schedule)
        return set_error(err, UC_E_OOM, "v2 registry schedule allocation failed");
    memcpy(schedule, plan->schedule, out->schedule_len * sizeof(*schedule));
    out->schedule = schedule;
    return UC_OK;
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
        const atom_field_desc_t *field =
            find_field_in_list(node->input_fields, node->input_fields_len, node->in[i].key);
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
    return node && node->atom_name && strcmp(node->atom_name, "mix_matrix") == 0;
}

static uc_status fill_scalar_refreshes(
    uc_arena                          *arena,
    const apg_v2_compiled_node_t      *node,
    apg_v2_registry_scalar_refresh_t **out_items,
    size_t                            *out_len,
    bool                               config,
    uc_error                          *err
) {
    size_t len = config ? count_config_refreshes(node) : count_input_refreshes(node);
    *out_items = NULL;
    *out_len   = len;
    if (len == 0u)
        return UC_OK;

    apg_v2_registry_scalar_refresh_t *items = uc_arena_alloc(arena, len * sizeof(*items), sizeof(void *));
    if (!items)
        return set_error(err, UC_E_OOM, "v2 registry scalar refresh allocation failed");

    size_t                           item_index  = 0u;
    size_t                           binding_len = config ? node->config_len : node->in_len;
    const apg_v2_compiled_binding_t *bindings    = config ? node->config : node->in;
    for (size_t i = 0; i < binding_len; i++) {
        if (config && bindings[i].kind == APG_BIND_FLOAT_MATRIX)
            continue;
        const atom_field_desc_t *field =
            config ? find_field_in_list(node->config_fields, node->config_fields_len, bindings[i].key)
                   : find_field_in_list(node->input_fields, node->input_fields_len, bindings[i].key);
        if (!field && !config)
            continue;
        if (!config && !scalar_refresh_field(field))
            continue;
        if (!field) {
            char msg[192];
            snprintf(
                msg, sizeof(msg), "node '%s' atom '%s' %s binding key '%s' metadata is missing",
                node->id ? node->id : "", node->atom_name ? node->atom_name : "", config ? "config" : "input",
                bindings[i].key ? bindings[i].key : ""
            );
            return set_error(err, UC_E_MISSING, msg);
        }
        if (!scalar_refresh_field(field))
            return set_error(err, UC_E_TYPE, "v2 registry scalar refresh field type is unsupported");
        items[item_index].key            = bindings[i].key;
        items[item_index].kind           = bindings[i].kind;
        items[item_index].param_index    = bindings[i].index;
        items[item_index].number         = bindings[i].number;
        items[item_index].storage_offset = field->offset;
        items[item_index].field_type     = field->type;
        items[item_index].config         = config;
        item_index++;
    }

    *out_items = items;
    return UC_OK;
}

static uc_status fill_signal_bindings(
    uc_arena                         *arena,
    size_t                            signals_len,
    const apg_v2_compiled_node_t     *node,
    const apg_v2_compiled_binding_t  *bindings,
    size_t                            bindings_len,
    bool                              is_input,
    size_t                           *array_cursor,
    apg_v2_registry_signal_binding_t *items,
    size_t                            items_cap,
    size_t                           *items_len,
    apg_v2_registry_node_layout_t    *layout,
    size_t                            node_array_base,
    uc_error                         *err
) {
    size_t len = 0u;
    for (size_t i = 0; i < bindings_len; i++) {
        if (bindings[i].kind == APG_BIND_SIGNAL || bindings[i].kind == APG_BIND_SIGNAL_ARRAY)
            len++;
    }

    if (len == 0u)
        return UC_OK;
    if (!items || !items_len)
        return set_error(err, UC_E_MISSING, "v2 registry signal binding output buffer is missing");
    if (*items_len > SIZE_MAX - len || *items_len + len > items_cap)
        return set_error(err, UC_E_RANGE, "v2 registry signal binding layout is inconsistent");
    if (!array_cursor)
        return set_error(err, UC_E_RANGE, "v2 registry signal array cursor is missing");
    if (*array_cursor < node_array_base)
        return set_error(err, UC_E_RANGE, "v2 registry signal binding layout is inconsistent");

    size_t local_array_cursor = 0u;
    size_t item_index         = 0u;
    for (size_t i = 0; i < bindings_len; i++) {
        const apg_v2_compiled_binding_t *binding = &bindings[i];
        if (binding->kind != APG_BIND_SIGNAL && binding->kind != APG_BIND_SIGNAL_ARRAY)
            continue;

        apg_v2_registry_signal_binding_t item = {0};
        item.key                              = binding->key;
        item.is_input                         = is_input;
        item.storage_offset                   = i * sizeof(float *);
        item.signal_array_len                 = 0u;

        if (is_input) {
            const atom_field_desc_t *field =
                find_field_in_list(node->input_fields, node->input_fields_len, binding->key);
            if (field) {
                if (field->type != FIELD_SIGNAL) {
                    if (binding->kind == APG_BIND_SIGNAL)
                        return set_error(err, UC_E_TYPE, "v2 registry input binding field is not a signal");
                    return set_error(err, UC_E_TYPE, "v2 registry input binding field does not support signal arrays");
                }
                item.storage_offset = field->offset;
            }
            if (binding->kind == APG_BIND_SIGNAL) {
                if (binding->index >= signals_len)
                    return set_error(err, UC_E_MISSING, "v2 registry input binding references invalid signal index");
                item.signal_index = binding->index;
            } else {
                item.is_signal_array  = true;
                item.signal_index     = SIZE_MAX;
                item.signal_array_len = binding->indices_len;
                if (binding->indices_len > SIZE_MAX - local_array_cursor)
                    return set_error(err, UC_E_RANGE, "v2 registry signal binding layout is too large");
                item.signal_array_offset = *array_cursor - node_array_base + local_array_cursor;
                if (binding->indices_len == 0u)
                    return set_error(err, UC_E_RANGE, "v2 registry input binding has empty signal array");
                item.signal_array_indices =
                    uc_arena_alloc(arena, binding->indices_len * sizeof(*item.signal_array_indices), sizeof(size_t));
                if (!item.signal_array_indices)
                    return set_error(err, UC_E_OOM, "v2 registry signal array index allocation failed");
                for (size_t j = 0; j < binding->indices_len; j++) {
                    if (binding->indices[j] >= signals_len)
                        return set_error(
                            err, UC_E_MISSING, "v2 registry input binding references invalid signal index"
                        );
                    item.signal_array_indices[j] = binding->indices[j];
                }
                local_array_cursor += binding->indices_len;
            }
        } else {
            if (binding->kind == APG_BIND_SIGNAL) {
                if (binding->index >= signals_len)
                    return set_error(err, UC_E_MISSING, "v2 registry output binding references invalid signal index");
                item.signal_index = binding->index;
            } else {
                item.is_signal_array  = true;
                item.signal_index     = SIZE_MAX;
                item.signal_array_len = binding->indices_len;
                if (binding->indices_len > SIZE_MAX - local_array_cursor)
                    return set_error(err, UC_E_RANGE, "v2 registry signal binding layout is too large");
                item.signal_array_offset = *array_cursor - node_array_base + local_array_cursor;
                if (binding->indices_len == 0u)
                    return set_error(err, UC_E_RANGE, "v2 registry output binding has empty signal array");
                item.signal_array_indices =
                    uc_arena_alloc(arena, binding->indices_len * sizeof(*item.signal_array_indices), sizeof(size_t));
                if (!item.signal_array_indices)
                    return set_error(err, UC_E_OOM, "v2 registry signal array index allocation failed");
                for (size_t j = 0; j < binding->indices_len; j++) {
                    if (binding->indices[j] >= signals_len)
                        return set_error(
                            err, UC_E_MISSING, "v2 registry output binding references invalid signal index"
                        );
                    item.signal_array_indices[j] = binding->indices[j];
                }
                local_array_cursor += binding->indices_len;
            }
        }

        items[*items_len + item_index++] = item;
    }

    if (local_array_cursor > layout->signal_array_pointer_slots)
        return set_error(err, UC_E_RANGE, "v2 registry signal binding layout is inconsistent");
    if (item_index != len)
        return set_error(err, UC_E_RANGE, "v2 registry signal binding layout is inconsistent");
    if (local_array_cursor > SIZE_MAX - *array_cursor)
        return set_error(err, UC_E_RANGE, "v2 registry signal array cursor overflow");
    *array_cursor += local_array_cursor;
    *items_len += item_index;
    return UC_OK;
}

static uc_status fill_mix_matrix_layout(
    uc_arena *arena, const apg_v2_compiled_node_t *node, apg_v2_registry_node_layout_t *layout, uc_error *err
) {
    if (!is_mix_matrix_node(node))
        return UC_OK;

    const apg_v2_compiled_binding_t *matrix = find_compiled_binding(node->config, node->config_len, "coefficients");
    if (!matrix || matrix->kind != APG_BIND_FLOAT_MATRIX)
        return set_error(err, UC_E_TYPE, "v2 registry mix_matrix requires coefficient matrix binding");
    if (matrix->rows == 0u || matrix->cols == 0u)
        return set_error(err, UC_E_RANGE, "v2 registry mix_matrix requires non-empty coefficient matrix");
    if (matrix->numbers == NULL)
        return set_error(err, UC_E_MISSING, "v2 registry mix_matrix coefficient data is missing");

    if (matrix->rows > SIZE_MAX / matrix->cols)
        return set_error(err, UC_E_RANGE, "v2 registry mix_matrix matrix size overflow");

    size_t coefficient_count            = matrix->rows * matrix->cols;
    layout->mix_matrix_coefficients_len = coefficient_count;
    layout->mix_matrix_num_out          = matrix->rows;
    layout->mix_matrix_num_in           = matrix->cols;

    layout->mix_matrix_coefficients =
        uc_arena_alloc(arena, coefficient_count * sizeof(*layout->mix_matrix_coefficients), sizeof(float));
    if (!layout->mix_matrix_coefficients)
        return set_error(err, UC_E_OOM, "v2 registry mix_matrix coefficient allocation failed");
    memcpy(layout->mix_matrix_coefficients, matrix->numbers, coefficient_count * sizeof(float));

    layout->mix_matrix_row_pointers =
        uc_arena_alloc(arena, matrix->rows * sizeof(*layout->mix_matrix_row_pointers), sizeof(float *));
    if (!layout->mix_matrix_row_pointers)
        return set_error(err, UC_E_OOM, "v2 registry mix_matrix row pointer allocation failed");

    for (size_t row = 0u; row < matrix->rows; row++)
        layout->mix_matrix_row_pointers[row] = &layout->mix_matrix_coefficients[row * matrix->cols];

    return UC_OK;
}

static uc_status
fill_control_targets(uc_arena *arena, const apg_unit_v2_t *unit, apg_v2_registry_t *out, uc_error *err) {
    out->control_targets_len = control_port_count(unit->input_ports, unit->input_ports_len);
    if (out->control_targets_len == 0u)
        return UC_OK;

    out->control_targets =
        uc_arena_alloc(arena, out->control_targets_len * sizeof(*out->control_targets), sizeof(void *));
    if (!out->control_targets)
        return set_error(err, UC_E_OOM, "v2 registry control target allocation failed");

    size_t target_index = 0u;
    for (size_t i = 0; i < unit->input_ports_len; i++) {
        const apg_unit_v2_port_t *port = &unit->input_ports[i];
        if (!port->type || strcmp(port->type, "control") != 0)
            continue;
        if (port->target_kind && strcmp(port->target_kind, "param") != 0)
            return set_error(err, UC_E_TYPE, "v2 registry only supports param control targets");
        const char *target =
            port->target_name ? port->target_name : (port->target_param ? port->target_param : port->name);
        int index = param_index_by_name(unit, target);
        if (index < 0)
            return set_error(err, UC_E_MISSING, "v2 registry control target param is missing");
        out->control_targets[target_index].port_name   = port->name;
        out->control_targets[target_index].param_name  = target;
        out->control_targets[target_index].param_index = (size_t)index;
        target_index++;
    }
    return UC_OK;
}

static uc_status
fill_node_layouts(uc_arena *arena, const apg_v2_compiled_unit_t *plan, apg_v2_registry_t *out, uc_error *err) {
    out->nodes_len = plan->nodes_len;
    if (out->nodes_len == 0u)
        return UC_OK;

    out->node_layouts = uc_arena_alloc(arena, out->nodes_len * sizeof(*out->node_layouts), sizeof(void *));
    if (!out->node_layouts)
        return set_error(err, UC_E_OOM, "v2 registry node layout allocation failed");

    size_t atom_storage_cursor       = 0u;
    size_t state_buffer_cursor       = 0u;
    size_t state_buffer_table_offset = 0u;
    size_t signal_array_cursor       = 0u;
    for (size_t node_index = 0; node_index < out->nodes_len; node_index++) {
        const apg_v2_compiled_node_t  *node   = &plan->nodes[node_index];
        apg_v2_registry_node_layout_t *layout = &out->node_layouts[node_index];
        if (!node->atom_name || !node->thunk)
            return set_error(err, UC_E_MISSING, "v2 registry node is missing compiled atom layout");

        memset(layout, 0, sizeof(*layout));

        layout->node_id        = node->id;
        layout->atom_name      = node->atom_name;
        layout->thunk          = node->thunk;
        layout->state_fields   = node->state_fields;
        layout->n_state_fields = node->state_fields_len > (size_t)INT_MAX ? INT_MAX : (int)node->state_fields_len;
        layout->out_size       = atom_storage_size(node->out_size);
        layout->in_size        = atom_storage_size(node->in_size);
        layout->config_size    = atom_storage_size(node->config_size);
        layout->state_size     = atom_storage_size(node->state_size);
        uc_status status       = reserve_storage(layout->out_size, &atom_storage_cursor, &layout->out_offset, err);
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
            signal_array_pointer_slots(node->in, node->in_len) + signal_array_pointer_slots(node->out, node->out_len);
        layout->signal_array_pool_offset = signal_array_cursor;

        for (size_t field_index = 0; field_index < node->state_fields_len; field_index++) {
            if (node->state_fields[field_index].type == FIELD_BUFFER)
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
                return set_error(err, UC_E_OOM, "v2 registry state buffer layout allocation failed");
        }

        size_t buffer_index = 0u;
        for (size_t field_index = 0; field_index < node->state_fields_len; field_index++) {
            if (node->state_fields[field_index].type != FIELD_BUFFER)
                continue;
            layout->state_buffer_samples_by_index[buffer_index++] = node->state_fields[field_index].buffer_samples;
            layout->state_buffer_sample_offsets_by_index[buffer_index - 1u] = state_buffer_cursor;
            if (node->state_fields[field_index].buffer_samples > SIZE_MAX - state_buffer_cursor)
                return set_error(err, UC_E_RANGE, "v2 registry state buffer layout is too large");
            state_buffer_cursor += node->state_fields[field_index].buffer_samples;
            layout->state_buffer_samples += node->state_fields[field_index].buffer_samples;
        }
        if (state_buffer_table_offset > SIZE_MAX - layout->state_buffers_len)
            return set_error(err, UC_E_RANGE, "v2 registry state-buffer table is too large");
        layout->state_buffer_table_offset = state_buffer_table_offset;
        state_buffer_table_offset += layout->state_buffers_len;
        if (layout->state_buffer_samples > SIZE_MAX - out->state_buffer_samples)
            return set_error(err, UC_E_RANGE, "v2 registry state-buffer samples is too large");
        out->state_buffers_len += layout->state_buffers_len;
        out->state_buffer_samples += layout->state_buffer_samples;

        status =
            fill_scalar_refreshes(arena, node, &layout->config_refreshes, &layout->config_refreshes_len, true, err);
        if (status != UC_OK)
            return status;
        status = fill_scalar_refreshes(arena, node, &layout->input_refreshes, &layout->input_refreshes_len, false, err);
        if (status != UC_OK)
            return status;

        size_t signal_bindings_len =
            count_signal_bindings(node->out, node->out_len) + count_signal_bindings(node->in, node->in_len);
        layout->signal_bindings_len = signal_bindings_len;
        if (signal_bindings_len > 0u) {
            layout->signal_bindings =
                uc_arena_alloc(arena, signal_bindings_len * sizeof(*layout->signal_bindings), sizeof(void *));
            if (!layout->signal_bindings)
                return set_error(err, UC_E_OOM, "v2 registry signal binding allocation failed");
        }

        size_t signal_binding_index = 0u;

        status = fill_signal_bindings(
            arena, out->signals_len, node, node->out, node->out_len, false, &signal_array_cursor,
            layout->signal_bindings, layout->signal_bindings_len, &signal_binding_index, layout,
            layout->signal_array_pool_offset, err
        );
        if (status != UC_OK)
            return status;

        status = fill_signal_bindings(
            arena, out->signals_len, node, node->in, node->in_len, true, &signal_array_cursor, layout->signal_bindings,
            layout->signal_bindings_len, &signal_binding_index, layout, layout->signal_array_pool_offset, err
        );
        if (status != UC_OK)
            return status;
        if (signal_binding_index != layout->signal_bindings_len)
            return set_error(err, UC_E_RANGE, "v2 registry signal binding layout is inconsistent");

        if (signal_array_cursor - layout->signal_array_pool_offset > layout->signal_array_pointer_slots)
            return set_error(err, UC_E_RANGE, "v2 registry signal binding layout is inconsistent");

        status = fill_mix_matrix_layout(arena, node, layout, err);
        if (status != UC_OK)
            return status;
    }
    out->signal_array_pointer_slots = signal_array_cursor;
    out->state_buffer_samples       = state_buffer_cursor;
    out->atom_storage_bytes         = atom_storage_cursor;
    return UC_OK;
}

uc_status apg_v2_registry_build(
    const apg_v2_compiled_unit_t *plan,
    uint32_t                      frame_capacity,
    float                         sample_rate,
    uc_arena                     *arena,
    apg_v2_registry_t            *out,
    uc_error                     *err
) {
    if (!plan || !plan->unit || !arena || !out || !err)
        return UC_E_TYPE;
    memset(out, 0, sizeof(*out));
    err->status = UC_OK;
    if (frame_capacity == 0u)
        return set_error(err, UC_E_RANGE, "v2 registry frame capacity must be greater than zero");
    if (plan->unit->signals_len > 0u && plan->unit->signals_len > SIZE_MAX / (size_t)frame_capacity)
        return set_error(err, UC_E_RANGE, "v2 registry signal layout is too large");
    if (plan->schedule_len > 0u && !plan->schedule)
        return set_error(err, UC_E_MISSING, "v2 registry schedule is missing");

    out->frame_capacity    = frame_capacity;
    out->sample_rate       = sample_rate > 0.0f ? sample_rate : 48000.0f;
    out->signals_len       = plan->unit->signals_len;
    out->signal_samples    = plan->unit->signals_len * (size_t)frame_capacity;
    out->params_len        = plan->unit->params_len;
    out->input_meters_len  = audio_port_meter_count(plan->unit->input_ports, plan->unit->input_ports_len);
    out->output_meters_len = audio_port_meter_count(plan->unit->output_ports, plan->unit->output_ports_len);
    out->schedule_len      = plan->schedule_len;

    uc_status status = fill_signal_names(arena, plan->unit, out, err);
    if (status != UC_OK)
        return status;

    status = fill_schedule(arena, plan, out, err);
    if (status != UC_OK)
        return status;

    status = fill_audio_port_map(
        arena, plan->unit, plan->unit->input_ports, plan->unit->input_ports_len, &out->input_audio_ports,
        &out->input_audio_ports_len, err
    );
    if (status != UC_OK)
        return status;

    status = fill_audio_port_map(
        arena, plan->unit, plan->unit->output_ports, plan->unit->output_ports_len, &out->output_audio_ports,
        &out->output_audio_ports_len, err
    );
    if (status != UC_OK)
        return status;

    status = fill_node_layouts(arena, plan, out, err);
    if (status != UC_OK)
        return status;

    status = fill_control_targets(arena, plan->unit, out, err);
    if (status != UC_OK)
        return status;

    status = fill_bypass_metadata(arena, plan, out, err);
    if (status != UC_OK)
        return status;

    status = fill_project_mute_output_indices(arena, plan->unit, out, err);
    if (status != UC_OK)
        return status;

    status = fill_param_names(arena, plan->unit, out, err);
    if (status != UC_OK)
        return status;

    if (out->params_len == 0u)
        return UC_OK;
    out->param_defaults         = uc_arena_alloc(arena, out->params_len * sizeof(*out->param_defaults), sizeof(float));
    out->param_smoothing_frames = uc_arena_alloc(
        arena, out->params_len * sizeof(*out->param_smoothing_frames), sizeof(*out->param_smoothing_frames)
    );
    if (!out->param_defaults || !out->param_smoothing_frames)
        return set_error(err, UC_E_OOM, "v2 registry param metadata allocation failed");
    for (size_t i = 0; i < out->params_len; i++) {
        out->param_defaults[i]         = parse_param_default(&plan->unit->params[i]);
        out->param_smoothing_frames[i] = param_smoothing_frames(&plan->unit->params[i], out->sample_rate);
    }
    return UC_OK;
}

uc_status apg_v2_registry_build_with_growth(
    const apg_v2_compiled_unit_t *plan,
    uint32_t                      frame_capacity,
    float                         sample_rate,
    uc_arena                     *out_arena,
    apg_v2_registry_t            *out_registry,
    uc_error                     *err
) {
    if (!plan || !out_arena || !out_registry || !err)
        return UC_E_TYPE;

    *out_arena = (uc_arena){0};
    memset(out_registry, 0, sizeof(*out_registry));

    size_t registry_arena_size = 4096u;
    while (registry_arena_size > 0u && registry_arena_size <= (SIZE_MAX >> 1)) {
        uc_arena registry_arena = {0};
        if (uc_arena_init(&registry_arena, registry_arena_size) != 0) {
            return set_error(err, UC_E_OOM, "v2 registry arena allocation failed");
        }

        uc_status status = apg_v2_registry_build(plan, frame_capacity, sample_rate, &registry_arena, out_registry, err);
        if (status == UC_OK) {
            *out_arena = registry_arena;
            return UC_OK;
        }

        uc_arena_free(&registry_arena);
        if (status != UC_E_OOM)
            return status;
        registry_arena_size *= 2u;
    }

    return set_error(err, UC_E_OOM, "v2 registry arena growth overflow");
}
