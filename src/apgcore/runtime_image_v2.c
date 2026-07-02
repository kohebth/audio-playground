#include <apgcore/runtime_image_v2.h>

#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static uc_status set_error(uc_error *err, uc_status status, const char *msg) {
    uc_loc loc = {0, 0};
    uc_error_set(err, status, loc, "%s", msg);
    return status;
}

static size_t atom_storage_size(size_t size) { return size > 0u ? size : 1u; }

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

    for (size_t node_index = 0; node_index < out->nodes_len; node_index++) {
        const atom_registry_entry_t  *atom   = out->plan->nodes[node_index].atom;
        apg_v2_runtime_node_layout_t *layout = &out->node_layouts[node_index];
        if (!atom)
            return set_error(err, UC_E_MISSING, "v2 runtime image node is missing atom metadata");

        layout->out_size    = atom_storage_size(atom->out_size);
        layout->in_size     = atom_storage_size(atom->in_size);
        layout->config_size = atom_storage_size(atom->config_size);
        layout->state_size  = atom_storage_size(atom->state_size);
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
    }
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
