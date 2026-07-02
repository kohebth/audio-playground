#include <apgcore/measure_v2.h>
#include <apgcore/runtime_v2.h>
#include <atom/dsp_types.h>

#include <limits.h>
#include <math.h>
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

static void runtime_set_error(apg_v2_runtime_t *runtime, const char *msg) {
    if (!runtime || !msg)
        return;
    snprintf(runtime->last_error, sizeof(runtime->last_error), "%s", msg);
}

static int signal_index_by_name(const apg_unit_v2_t *unit, const char *name) {
    if (!unit || !name)
        return -1;
    for (size_t i = 0; i < unit->signals_len; i++) {
        if (unit->signals[i] && strcmp(unit->signals[i], name) == 0)
            return (int)i;
    }
    return -1;
}

static const char *first_audio_port_name(const apg_unit_v2_port_t *ports, size_t ports_len) {
    for (size_t i = 0; i < ports_len; i++) {
        if (ports[i].type && strcmp(ports[i].type, "audio") == 0)
            return ports[i].name;
    }
    return NULL;
}

static const apg_unit_v2_port_t *
audio_port_by_name(const apg_unit_v2_port_t *ports, size_t ports_len, const char *port_name) {
    if (!ports || !port_name)
        return NULL;
    for (size_t i = 0; i < ports_len; i++) {
        if (!ports[i].name || strcmp(ports[i].name, port_name) != 0)
            continue;
        if (!ports[i].type || strcmp(ports[i].type, "audio") != 0)
            return NULL;
        return &ports[i];
    }
    return NULL;
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

static const char *audio_port_channel_signal_name(const apg_unit_v2_port_t *port, size_t channel_index) {
    if (!port)
        return NULL;
    if (port->signals_len > 0u)
        return channel_index < port->signals_len ? port->signals[channel_index] : NULL;
    return channel_index == 0u ? port->name : NULL;
}

static int audio_port_signal_index_by_channel_name(
    const apg_unit_v2_t      *unit,
    const apg_unit_v2_port_t *ports,
    size_t                    ports_len,
    const char               *port_name,
    size_t                    channel_index
) {
    const apg_unit_v2_port_t *port = audio_port_by_name(ports, ports_len, port_name);
    if (!unit || !port)
        return -1;
    return signal_index_by_name(unit, audio_port_channel_signal_name(port, channel_index));
}

static int audio_port_signal_index_by_name(
    const apg_unit_v2_t *unit, const apg_unit_v2_port_t *ports, size_t ports_len, const char *port_name
) {
    return audio_port_signal_index_by_channel_name(unit, ports, ports_len, port_name, 0u);
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

static bool node_id_has_instance_prefix(const char *node_id, const char *instance_id) {
    if (!node_id || !instance_id || instance_id[0] == '\0')
        return false;
    size_t len = strlen(instance_id);
    return strncmp(node_id, instance_id, len) == 0 && node_id[len] == '.';
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

static bool binding_signal_index(const apg_v2_compiled_binding_t *binding, size_t offset, size_t *out_index) {
    if (!binding || !out_index)
        return false;
    if (binding->kind == APG_BIND_SIGNAL && offset == 0u) {
        *out_index = binding->index;
        return true;
    }
    if (binding->kind == APG_BIND_SIGNAL_ARRAY && offset < binding->indices_len) {
        *out_index = binding->indices[offset];
        return true;
    }
    return false;
}

static bool node_outputs_signal(const apg_v2_compiled_node_t *node, size_t signal_index) {
    if (!node)
        return false;
    for (size_t i = 0; i < node->out_len; i++) {
        for (size_t j = 0; j < binding_signal_count(&node->out[i]); j++) {
            size_t index = 0;
            if (binding_signal_index(&node->out[i], j, &index) && index == signal_index)
                return true;
        }
    }
    return false;
}

static bool node_inputs_signal(const apg_v2_compiled_node_t *node, size_t signal_index) {
    if (!node)
        return false;
    for (size_t i = 0; i < node->in_len; i++) {
        for (size_t j = 0; j < binding_signal_count(&node->in[i]); j++) {
            size_t index = 0;
            if (binding_signal_index(&node->in[i], j, &index) && index == signal_index)
                return true;
        }
    }
    return false;
}

static bool instance_outputs_signal(const apg_v2_runtime_t *runtime, const char *instance_id, size_t signal_index) {
    if (!runtime || !runtime->plan)
        return false;
    for (size_t i = 0; i < runtime->plan->nodes_len; i++) {
        const apg_v2_compiled_node_t *node = &runtime->plan->nodes[i];
        if (node_id_has_instance_prefix(node->id, instance_id) && node_outputs_signal(node, signal_index))
            return true;
    }
    return false;
}

static bool
signal_consumed_outside_instance(const apg_v2_runtime_t *runtime, const char *instance_id, size_t signal_index) {
    if (!runtime || !runtime->plan)
        return false;
    for (size_t i = 0; i < runtime->plan->nodes_len; i++) {
        const apg_v2_compiled_node_t *node = &runtime->plan->nodes[i];
        if (!node_id_has_instance_prefix(node->id, instance_id) && node_inputs_signal(node, signal_index))
            return true;
    }
    return false;
}

static bool signal_is_public_output(const apg_v2_runtime_t *runtime, size_t signal_index) {
    if (!runtime || !runtime->plan || !runtime->plan->unit)
        return false;
    const apg_unit_v2_t *unit = runtime->plan->unit;
    for (size_t i = 0; i < unit->output_ports_len; i++) {
        const apg_unit_v2_port_t *port = &unit->output_ports[i];
        if (!port || !port->type || strcmp(port->type, "audio") != 0)
            continue;
        size_t channels = 0;
        if (!parse_port_channel_count(port, &channels))
            continue;
        for (size_t ch = 0; ch < channels; ch++) {
            int index = signal_index_by_name(unit, audio_port_channel_signal_name(port, ch));
            if (index >= 0 && (size_t)index == signal_index)
                return true;
        }
    }
    return false;
}

static uint32_t param_smoothing_frames(const apg_unit_v2_param_t *param, const apg_v2_runtime_t *runtime) {
    if (!param || !param->smoothing_ms || !runtime)
        return 0u;
    float smoothing_ms = strtof(param->smoothing_ms, NULL);
    if (smoothing_ms <= 0.0f)
        return 0u;
    double sample_rate = runtime->process_info.sample_rate > 0.0f ? runtime->process_info.sample_rate : 48000.0f;
    double frames      = ((double)smoothing_ms * sample_rate) / 1000.0;
    if (frames >= (double)UINT32_MAX)
        return UINT32_MAX;
    uint32_t rounded = (uint32_t)(frames + 0.999999);
    return rounded > 0u ? rounded : 1u;
}

static uc_status init_signal_buffers(const apg_v2_runtime_image_t *image, apg_v2_runtime_t *out, uc_error *err) {
    out->signals_len = image->signals_len;
    if (out->signals_len == 0u)
        return UC_OK;
    if (image->signal_samples > SIZE_MAX / sizeof(float))
        return set_error(err, UC_E_RANGE, "v2 runtime signal pool is too large");

    out->signals     = calloc(out->signals_len, sizeof(*out->signals));
    out->signal_pool = calloc(image->signal_samples, sizeof(*out->signal_pool));
    if (!out->signals || !out->signal_pool)
        return set_error(err, UC_E_OOM, "v2 runtime signal allocation failed");

    for (size_t i = 0; i < out->signals_len; i++)
        out->signals[i] = &out->signal_pool[i * (size_t)image->frame_capacity];
    return UC_OK;
}

static uc_status init_meters(const apg_v2_runtime_image_t *image, apg_v2_runtime_t *out, uc_error *err) {
    out->input_meters_len = image->input_meters_len;
    if (out->input_meters_len > 0u) {
        out->input_meters = calloc(out->input_meters_len, sizeof(*out->input_meters));
        if (!out->input_meters)
            return set_error(err, UC_E_OOM, "v2 runtime input meter allocation failed");
    }

    out->output_meters_len = image->output_meters_len;
    if (out->output_meters_len > 0u) {
        out->output_meters = calloc(out->output_meters_len, sizeof(*out->output_meters));
        if (!out->output_meters)
            return set_error(err, UC_E_OOM, "v2 runtime output meter allocation failed");
    }
    return UC_OK;
}

static uc_status init_params(const apg_v2_runtime_image_t *image, apg_v2_runtime_t *out, uc_error *err) {
    out->params_len = image->params_len;
    if (out->params_len == 0u)
        return UC_OK;

    out->params                           = calloc(out->params_len, sizeof(*out->params));
    out->param_defaults                   = calloc(out->params_len, sizeof(*out->param_defaults));
    out->param_targets                    = calloc(out->params_len, sizeof(*out->param_targets));
    out->param_smoothing_remaining_frames = calloc(out->params_len, sizeof(*out->param_smoothing_remaining_frames));
    if (!out->params || !out->param_defaults || !out->param_targets || !out->param_smoothing_remaining_frames)
        return set_error(err, UC_E_OOM, "v2 runtime param allocation failed");

    for (size_t i = 0; i < out->params_len; i++) {
        out->param_defaults[i] = image->param_defaults ? image->param_defaults[i] : 0.0f;
        out->params[i]         = out->param_defaults[i];
        out->param_targets[i]  = out->params[i];
    }
    return UC_OK;
}

static uc_status init_control_targets(const apg_v2_runtime_image_t *image, apg_v2_runtime_t *out, uc_error *err) {
    out->control_targets_len = image->control_targets_len;
    if (out->control_targets_len == 0u)
        return UC_OK;
    out->control_targets = calloc(out->control_targets_len, sizeof(*out->control_targets));
    if (!out->control_targets)
        return set_error(err, UC_E_OOM, "v2 runtime control target allocation failed");
    memcpy(out->control_targets, image->control_targets, out->control_targets_len * sizeof(*out->control_targets));
    return UC_OK;
}

static void advance_smoothed_params(apg_v2_runtime_t *runtime, uint32_t frames) {
    if (!runtime || !runtime->params || !runtime->param_targets || !runtime->param_smoothing_remaining_frames ||
        !runtime->plan || !runtime->plan->unit)
        return;
    for (size_t i = 0; i < runtime->params_len && i < runtime->plan->unit->params_len; i++) {
        uint32_t remaining = runtime->param_smoothing_remaining_frames[i];
        if (remaining == 0u)
            continue;
        uint32_t advance = frames < remaining ? frames : remaining;
        runtime->params[i] += (runtime->param_targets[i] - runtime->params[i]) * ((float)advance / (float)remaining);
        remaining -= advance;
        runtime->param_smoothing_remaining_frames[i] = remaining;
        if (remaining == 0u)
            runtime->params[i] = runtime->param_targets[i];
    }
}

static uc_status
runtime_node_aux_alloc(apg_v2_runtime_node_t *node, size_t size, void **out_ptr, uc_error *err, const char *msg) {
    void **blocks = realloc(node->aux_blocks, (node->aux_blocks_len + 1u) * sizeof(*blocks));
    if (!blocks)
        return set_error(err, UC_E_OOM, msg ? msg : "v2 runtime aux allocation failed");
    void *block = calloc(1u, atom_storage_size(size));
    if (!block)
        return set_error(err, UC_E_OOM, msg ? msg : "v2 runtime aux allocation failed");
    blocks[node->aux_blocks_len++] = block;
    node->aux_blocks               = blocks;
    *out_ptr                       = block;
    return UC_OK;
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

static float compiled_scalar_value(const apg_v2_compiled_binding_t *binding, const apg_v2_runtime_t *runtime) {
    if (binding->kind == APG_BIND_PARAM)
        return binding->index < runtime->params_len ? runtime->params[binding->index] : 0.0f;
    if (binding->kind == APG_BIND_LITERAL)
        return binding->literal ? strtof(binding->literal, NULL) : 0.0f;
    return 0.0f;
}

static uc_status bind_input_field(
    const apg_v2_compiled_node_t    *compiled,
    const apg_v2_compiled_binding_t *binding,
    const atom_field_desc_t         *field,
    apg_v2_runtime_t                *out,
    void                            *storage,
    uc_error                        *err
) {
    void *addr = (char *)storage + field->offset;
    if (field->type == FIELD_SIGNAL) {
        if (binding->kind != APG_BIND_SIGNAL || binding->index >= out->signals_len) {
            char msg[192];
            snprintf(
                msg, sizeof(msg), "node '%s' atom '%s' in binding key '%s' references invalid signal index",
                compiled && compiled->id ? compiled->id : "", compiled && compiled->atom ? compiled->atom->name : "",
                binding->key ? binding->key : ""
            );
            return set_error(err, UC_E_MISSING, msg);
        }
        *(float **)addr = out->signals[binding->index];
        return UC_OK;
    }
    if (field->type == FIELD_INT) {
        if (binding->kind == APG_BIND_SIGNAL)
            return set_error(err, UC_E_TYPE, "v2 runtime scalar input cannot be bound to a signal");
        *(int *)addr = (int)compiled_scalar_value(binding, out);
        return UC_OK;
    }
    if (field->type == FIELD_FLOAT) {
        if (binding->kind == APG_BIND_SIGNAL)
            return set_error(err, UC_E_TYPE, "v2 runtime scalar input cannot be bound to a signal");
        *(float *)addr = compiled_scalar_value(binding, out);
        return UC_OK;
    }
    return set_error(err, UC_E_TYPE, "v2 runtime input field type is unsupported");
}

static uc_status bind_signal_fields(
    const apg_v2_compiled_node_t    *compiled,
    const char                      *section,
    const apg_v2_compiled_binding_t *bindings,
    size_t                           bindings_len,
    apg_v2_runtime_t                *out,
    apg_v2_runtime_node_t           *node,
    void                            *storage,
    uc_error                        *err
) {
    float **fields = (float **)storage;
    for (size_t i = 0; i < bindings_len; i++) {
        const atom_field_desc_t *input_field = section && strcmp(section, "in") == 0
                                                   ? find_input_field(compiled ? compiled->atom : NULL, bindings[i].key)
                                                   : NULL;
        if (input_field) {
            uc_status status = bind_input_field(compiled, &bindings[i], input_field, out, storage, err);
            if (status != UC_OK)
                return status;
            continue;
        }
        if (bindings[i].kind == APG_BIND_SIGNAL_ARRAY) {
            if (node->signal_array_pool_used + bindings[i].indices_len > node->signal_array_pool_len)
                return set_error(err, UC_E_RANGE, "v2 runtime signal array pool is too small");
            float **signal_array = &node->signal_array_pool[node->signal_array_pool_used];
            node->signal_array_pool_used += bindings[i].indices_len;
            for (size_t j = 0; j < bindings[i].indices_len; j++) {
                if (bindings[i].indices[j] >= out->signals_len) {
                    char msg[192];
                    snprintf(
                        msg, sizeof(msg), "node '%s' atom '%s' %s binding key '%s' references invalid signal index",
                        compiled && compiled->id ? compiled->id : "",
                        compiled && compiled->atom ? compiled->atom->name : "", section ? section : "binding",
                        bindings[i].key ? bindings[i].key : ""
                    );
                    return set_error(err, UC_E_MISSING, msg);
                }
                signal_array[j] = out->signals[bindings[i].indices[j]];
            }
            ((float ***)storage)[i] = signal_array;
            continue;
        }
        if (bindings[i].kind != APG_BIND_SIGNAL || bindings[i].index >= out->signals_len) {
            char msg[192];
            snprintf(
                msg, sizeof(msg), "node '%s' atom '%s' %s binding key '%s' references invalid signal index",
                compiled && compiled->id ? compiled->id : "", compiled && compiled->atom ? compiled->atom->name : "",
                section ? section : "binding", bindings[i].key ? bindings[i].key : ""
            );
            return set_error(err, UC_E_MISSING, msg);
        }
        fields[i] = out->signals[bindings[i].index];
    }
    return UC_OK;
}

static const atom_field_desc_t *find_config_field(const atom_registry_entry_t *atom, const char *key) {
    for (int i = 0; i < atom->n_config_fields; i++) {
        if (atom->config_fields[i].name && strcmp(atom->config_fields[i].name, key) == 0)
            return &atom->config_fields[i];
    }
    return NULL;
}

static float compiled_config_value(const apg_v2_compiled_binding_t *binding, const apg_v2_runtime_t *runtime) {
    return compiled_scalar_value(binding, runtime);
}

static uc_status refresh_node_config(const apg_v2_compiled_node_t *compiled, apg_v2_runtime_t *runtime, uc_error *err) {
    apg_v2_runtime_node_t *node = &runtime->nodes[compiled - runtime->plan->nodes];
    for (size_t i = 0; i < compiled->config_len; i++) {
        if (compiled->config[i].kind == APG_BIND_FLOAT_MATRIX)
            continue;
        const atom_field_desc_t *field = find_config_field(compiled->atom, compiled->config[i].key);
        if (!field) {
            char msg[192];
            snprintf(
                msg, sizeof(msg), "node '%s' atom '%s' config binding key '%s' metadata is missing",
                compiled->id ? compiled->id : "", compiled->atom ? compiled->atom->name : "",
                compiled->config[i].key ? compiled->config[i].key : ""
            );
            return set_error(err, UC_E_MISSING, msg);
        }

        void *addr  = (char *)node->config_storage + field->offset;
        float value = compiled_config_value(&compiled->config[i], runtime);
        if (field->type == FIELD_INT)
            *(int *)addr = (int)value;
        else if (field->type == FIELD_FLOAT)
            *(float *)addr = value;
        else {
            char msg[192];
            snprintf(
                msg, sizeof(msg), "node '%s' atom '%s' config binding key '%s' field type is not scalar",
                compiled->id ? compiled->id : "", compiled->atom ? compiled->atom->name : "",
                compiled->config[i].key ? compiled->config[i].key : ""
            );
            return set_error(err, UC_E_TYPE, msg);
        }
    }
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

static uc_status
bind_structured_config(const apg_v2_compiled_node_t *compiled, apg_v2_runtime_node_t *node, uc_error *err) {
    if (!compiled || !compiled->atom || !compiled->atom->name || strcmp(compiled->atom->name, "mix_matrix") != 0)
        return UC_OK;

    const apg_v2_compiled_binding_t *matrix =
        find_compiled_binding(compiled->config, compiled->config_len, "coefficients");
    if (!matrix || matrix->kind != APG_BIND_FLOAT_MATRIX)
        return set_error(err, UC_E_TYPE, "mix_matrix requires coefficient matrix binding");

    float   **rows   = NULL;
    uc_status status = runtime_node_aux_alloc(
        node, matrix->rows * sizeof(*rows), (void **)&rows, err, "v2 runtime coefficient matrix allocation failed"
    );
    if (status != UC_OK)
        return status;
    for (size_t row = 0; row < matrix->rows; row++)
        rows[row] = &matrix->numbers[row * matrix->cols];

    mix_matrix_params_t *params = (mix_matrix_params_t *)node->config_storage;
    params->coefficients        = rows;
    params->num_out             = (int)matrix->rows;
    params->num_in              = (int)matrix->cols;
    return UC_OK;
}

static uc_status
refresh_node_input_scalars(const apg_v2_compiled_node_t *compiled, apg_v2_runtime_t *runtime, uc_error *err) {
    apg_v2_runtime_node_t *node = &runtime->nodes[compiled - runtime->plan->nodes];
    for (size_t i = 0; i < compiled->in_len; i++) {
        const atom_field_desc_t *field = find_input_field(compiled->atom, compiled->in[i].key);
        if (!field || field->type == FIELD_SIGNAL)
            continue;
        uc_status status = bind_input_field(compiled, &compiled->in[i], field, runtime, node->in_storage, err);
        if (status != UC_OK)
            return status;
    }
    return UC_OK;
}

static uc_status init_state_buffers(
    const apg_v2_compiled_node_t       *compiled,
    const apg_v2_runtime_node_layout_t *layout,
    apg_v2_runtime_node_t              *node,
    uc_error                           *err
) {
    const atom_registry_entry_t *atom = compiled->atom;
    if (!layout || layout->state_buffers_len == 0u)
        return UC_OK;

    node->state_buffers        = calloc(layout->state_buffers_len, sizeof(*node->state_buffers));
    node->state_buffer_samples = calloc(layout->state_buffers_len, sizeof(*node->state_buffer_samples));
    if (!node->state_buffers || !node->state_buffer_samples) {
        char msg[192];
        snprintf(
            msg, sizeof(msg), "node '%s' atom '%s' state buffer allocation failed", compiled->id ? compiled->id : "",
            atom->name ? atom->name : ""
        );
        return set_error(err, UC_E_OOM, msg);
    }
    node->state_buffers_len = layout->state_buffers_len;

    size_t buffer_index = 0;
    for (int i = 0; i < atom->n_state_fields; i++) {
        const atom_field_desc_t *field = &atom->state_fields[i];
        if (field->type != FIELD_BUFFER)
            continue;
        size_t buffer_samples = layout->state_buffer_samples_by_index
                                    ? layout->state_buffer_samples_by_index[buffer_index]
                                    : field->buffer_samples;
        if (buffer_samples == 0u) {
            char msg[192];
            snprintf(
                msg, sizeof(msg), "node '%s' atom '%s' state binding key '%s' is missing buffer capacity",
                compiled->id ? compiled->id : "", atom->name ? atom->name : "", field->name ? field->name : ""
            );
            return set_error(err, UC_E_MISSING, msg);
        }
        float *buffer = calloc(buffer_samples, sizeof(*buffer));
        if (!buffer) {
            char msg[192];
            snprintf(
                msg, sizeof(msg), "node '%s' atom '%s' state binding key '%s' buffer allocation failed",
                compiled->id ? compiled->id : "", atom->name ? atom->name : "", field->name ? field->name : ""
            );
            return set_error(err, UC_E_OOM, msg);
        }
        node->state_buffers[buffer_index]        = buffer;
        node->state_buffer_samples[buffer_index] = buffer_samples;
        buffer_index++;
        float **field_ptr = (float **)((char *)node->state_storage + field->offset);
        *field_ptr        = buffer;
    }
    return UC_OK;
}

static uc_status init_node_calls(const apg_v2_runtime_image_t *image, apg_v2_runtime_t *out, uc_error *err) {
    const apg_v2_compiled_unit_t *plan = image->plan;

    out->nodes_len = image->nodes_len;
    if (out->nodes_len == 0u)
        return UC_OK;
    if (!image->node_layouts)
        return set_error(err, UC_E_MISSING, "v2 runtime image node layouts are missing");

    out->nodes = calloc(out->nodes_len, sizeof(*out->nodes));
    if (!out->nodes)
        return set_error(err, UC_E_OOM, "v2 runtime node allocation failed");

    for (size_t i = 0; i < out->nodes_len; i++) {
        const atom_registry_entry_t        *atom   = plan->nodes[i].atom;
        apg_v2_runtime_node_t              *node   = &out->nodes[i];
        const apg_v2_runtime_node_layout_t *layout = &image->node_layouts[i];
        if (!atom)
            return set_error(err, UC_E_MISSING, "v2 runtime node is missing atom metadata");

        node->out_storage    = calloc(1u, layout->out_size);
        node->in_storage     = calloc(1u, layout->in_size);
        node->config_storage = calloc(1u, layout->config_size);
        node->state_storage  = calloc(1u, layout->state_size);
        if (!node->out_storage || !node->in_storage || !node->config_storage || !node->state_storage)
            return set_error(err, UC_E_OOM, "v2 runtime atom call allocation failed");
        if (layout->signal_array_pointer_slots > 0u) {
            node->signal_array_pool = calloc(layout->signal_array_pointer_slots, sizeof(*node->signal_array_pool));
            if (!node->signal_array_pool)
                return set_error(err, UC_E_OOM, "v2 runtime signal array pool allocation failed");
            node->signal_array_pool_len = layout->signal_array_pointer_slots;
        }

        node->call.out    = node->out_storage;
        node->call.in     = node->in_storage;
        node->call.config = node->config_storage;
        node->call.state  = node->state_storage;
        node->call.info   = &out->process_info;

        uc_status status = init_state_buffers(&plan->nodes[i], layout, node, err);
        if (status != UC_OK)
            return status;
        status = bind_signal_fields(
            &plan->nodes[i], "out", plan->nodes[i].out, plan->nodes[i].out_len, out, node, node->out_storage, err
        );
        if (status != UC_OK)
            return status;
        status = bind_signal_fields(
            &plan->nodes[i], "in", plan->nodes[i].in, plan->nodes[i].in_len, out, node, node->in_storage, err
        );
        if (status != UC_OK)
            return status;
        status = refresh_node_config(&plan->nodes[i], out, err);
        if (status != UC_OK)
            return status;
        status = bind_structured_config(&plan->nodes[i], node, err);
        if (status != UC_OK)
            return status;
    }
    return UC_OK;
}

uc_status apg_v2_runtime_init_from_image(const apg_v2_runtime_image_t *image, apg_v2_runtime_t *out, uc_error *err) {
    if (!image || !image->plan || !image->plan->unit || !out || !err)
        return UC_E_TYPE;
    memset(out, 0, sizeof(*out));
    err->status = UC_OK;
    if (image->frame_capacity == 0u)
        return set_error(err, UC_E_RANGE, "v2 runtime frame capacity must be greater than zero");

    out->plan                       = image->plan;
    out->frame_capacity             = image->frame_capacity;
    out->process_info.sample_rate   = image->sample_rate;
    out->process_info.frames        = image->frame_capacity;
    out->process_info.output_frames = image->frame_capacity;
    out->process_info.channels      = 1u;

    uc_status status = init_signal_buffers(image, out, err);
    if (status != UC_OK)
        goto fail;
    status = init_meters(image, out, err);
    if (status != UC_OK)
        goto fail;
    status = init_params(image, out, err);
    if (status != UC_OK)
        goto fail;
    status = init_control_targets(image, out, err);
    if (status != UC_OK)
        goto fail;
    status = init_node_calls(image, out, err);
    if (status != UC_OK)
        goto fail;
    return UC_OK;

fail:
    apg_v2_runtime_destroy(out);
    return status;
}

uc_status apg_v2_runtime_init(
    const apg_v2_compiled_unit_t *plan, uint32_t frame_capacity, float sample_rate, apg_v2_runtime_t *out, uc_error *err
) {
    if (!plan || !plan->unit || !out || !err)
        return UC_E_TYPE;

    uc_arena image_arena;
    if (uc_arena_init(&image_arena, 4096) != 0)
        return set_error(err, UC_E_OOM, "v2 runtime image arena allocation failed");

    apg_v2_runtime_image_t image;
    uc_status status = apg_v2_runtime_image_build(plan, frame_capacity, sample_rate, &image_arena, &image, err);
    if (status == UC_OK)
        status = apg_v2_runtime_init_from_image(&image, out, err);
    uc_arena_free(&image_arena);
    return status;
}

float *apg_v2_runtime_find_signal(apg_v2_runtime_t *runtime, const char *name) {
    if (!runtime || !runtime->plan || !runtime->plan->unit || !name)
        return NULL;
    int index = signal_index_by_name(runtime->plan->unit, name);
    if (index < 0 || (size_t)index >= runtime->signals_len)
        return NULL;
    return runtime->signals[index];
}

float *apg_v2_runtime_find_input_port_signal(apg_v2_runtime_t *runtime, const char *port_name) {
    if (!runtime || !runtime->plan || !runtime->plan->unit || !port_name)
        return NULL;
    const apg_unit_v2_t *unit = runtime->plan->unit;
    int index = audio_port_signal_index_by_name(unit, unit->input_ports, unit->input_ports_len, port_name);
    if (index < 0 || (size_t)index >= runtime->signals_len)
        return NULL;
    return runtime->signals[index];
}

float *apg_v2_runtime_find_output_port_signal(apg_v2_runtime_t *runtime, const char *port_name) {
    if (!runtime || !runtime->plan || !runtime->plan->unit || !port_name)
        return NULL;
    const apg_unit_v2_t *unit = runtime->plan->unit;
    int index = audio_port_signal_index_by_name(unit, unit->output_ports, unit->output_ports_len, port_name);
    if (index < 0 || (size_t)index >= runtime->signals_len)
        return NULL;
    return runtime->signals[index];
}

float *
apg_v2_runtime_find_input_port_channel_signal(apg_v2_runtime_t *runtime, const char *port_name, size_t channel_index) {
    if (!runtime || !runtime->plan || !runtime->plan->unit || !port_name)
        return NULL;
    const apg_unit_v2_t *unit  = runtime->plan->unit;
    int                  index = audio_port_signal_index_by_channel_name(
        unit, unit->input_ports, unit->input_ports_len, port_name, channel_index
    );
    if (index < 0 || (size_t)index >= runtime->signals_len)
        return NULL;
    return runtime->signals[index];
}

float *
apg_v2_runtime_find_output_port_channel_signal(apg_v2_runtime_t *runtime, const char *port_name, size_t channel_index) {
    if (!runtime || !runtime->plan || !runtime->plan->unit || !port_name)
        return NULL;
    const apg_unit_v2_t *unit  = runtime->plan->unit;
    int                  index = audio_port_signal_index_by_channel_name(
        unit, unit->output_ports, unit->output_ports_len, port_name, channel_index
    );
    if (index < 0 || (size_t)index >= runtime->signals_len)
        return NULL;
    return runtime->signals[index];
}

static int runtime_instance_bypass_index(const apg_v2_runtime_t *runtime, const char *instance_id) {
    if (!runtime || !instance_id)
        return -1;
    for (size_t i = 0; i < runtime->bypassed_instances_len; i++) {
        if (runtime->bypassed_instances[i] && strcmp(runtime->bypassed_instances[i], instance_id) == 0)
            return (int)i;
    }
    return -1;
}

static int runtime_node_bypass_index(const apg_v2_runtime_t *runtime, const char *node_id) {
    if (!runtime || !node_id)
        return -1;
    for (size_t i = 0; i < runtime->bypassed_instances_len; i++) {
        if (node_id_has_instance_prefix(node_id, runtime->bypassed_instances[i]))
            return (int)i;
    }
    return -1;
}

static bool find_instance_bypass_io(
    const apg_v2_runtime_t *runtime, const char *instance_id, size_t *input_index, size_t *output_index
) {
    if (!runtime || !runtime->plan || !instance_id || !input_index || !output_index)
        return false;

    bool found_input  = false;
    bool found_output = false;
    for (size_t i = 0; i < runtime->plan->nodes_len; i++) {
        const apg_v2_compiled_node_t *node = &runtime->plan->nodes[i];
        if (!node_id_has_instance_prefix(node->id, instance_id))
            continue;
        for (size_t b = 0; b < node->in_len && !found_input; b++) {
            for (size_t s = 0; s < binding_signal_count(&node->in[b]); s++) {
                size_t index = 0;
                if (binding_signal_index(&node->in[b], s, &index) &&
                    !instance_outputs_signal(runtime, instance_id, index)) {
                    *input_index = index;
                    found_input  = true;
                    break;
                }
            }
        }
        for (size_t b = 0; b < node->out_len && !found_output; b++) {
            for (size_t s = 0; s < binding_signal_count(&node->out[b]); s++) {
                size_t index = 0;
                if (binding_signal_index(&node->out[b], s, &index) &&
                    (signal_consumed_outside_instance(runtime, instance_id, index) ||
                     signal_is_public_output(runtime, index))) {
                    *output_index = index;
                    found_output  = true;
                    break;
                }
            }
        }
    }
    return found_input && found_output && *input_index < runtime->signals_len && *output_index < runtime->signals_len;
}

static bool apply_instance_bypass(apg_v2_runtime_t *runtime, const char *instance_id, uint32_t frames) {
    size_t input_index  = 0u;
    size_t output_index = 0u;
    if (!find_instance_bypass_io(runtime, instance_id, &input_index, &output_index)) {
        runtime_set_error(
            runtime, "v2 runtime instance bypass requires one external input and one public output signal"
        );
        return false;
    }
    if (!runtime->signals[input_index] || !runtime->signals[output_index]) {
        runtime_set_error(runtime, "v2 runtime instance bypass signal lookup failed");
        return false;
    }
    memcpy(runtime->signals[output_index], runtime->signals[input_index], frames * sizeof(float));
    return true;
}

static apg_v2_meter_snapshot_t meter_snapshot_from_signal(const float *signal, uint32_t frames) {
    apg_v2_meter_snapshot_t snapshot = {0};
    if (!signal || frames == 0u)
        return snapshot;

    float  peak       = 0.0f;
    double sum_square = 0.0;
    for (uint32_t i = 0; i < frames; i++) {
        float sample     = signal[i];
        float abs_sample = fabsf(sample);
        if (abs_sample > peak)
            peak = abs_sample;
        sum_square += (double)sample * (double)sample;
    }
    snapshot.peak   = peak;
    snapshot.rms    = (float)sqrt(sum_square / (double)frames);
    snapshot.frames = frames;
    snapshot.valid  = true;
    return snapshot;
}

static void update_meters_for_ports(
    apg_v2_runtime_t         *runtime,
    const apg_unit_v2_port_t *ports,
    size_t                    ports_len,
    apg_v2_meter_snapshot_t  *meters,
    size_t                    meters_len,
    uint32_t                  frames
) {
    if (!runtime || !runtime->plan || !runtime->plan->unit || !meters)
        return;
    const apg_unit_v2_t *unit   = runtime->plan->unit;
    size_t               offset = 0u;
    for (size_t i = 0; i < ports_len; i++) {
        const apg_unit_v2_port_t *port = &ports[i];
        if (!port->type || strcmp(port->type, "audio") != 0)
            continue;
        size_t channels = 0u;
        if (!parse_port_channel_count(port, &channels))
            continue;
        for (size_t ch = 0; ch < channels && offset < meters_len; ch++, offset++) {
            int index      = signal_index_by_name(unit, audio_port_channel_signal_name(port, ch));
            meters[offset] = index >= 0 && (size_t)index < runtime->signals_len
                                 ? meter_snapshot_from_signal(runtime->signals[index], frames)
                                 : (apg_v2_meter_snapshot_t){0};
        }
    }
}

static void update_public_port_meters(apg_v2_runtime_t *runtime, uint32_t frames) {
    if (!runtime || !runtime->plan || !runtime->plan->unit)
        return;
    const apg_unit_v2_t *unit = runtime->plan->unit;
    update_meters_for_ports(
        runtime, unit->input_ports, unit->input_ports_len, runtime->input_meters, runtime->input_meters_len, frames
    );
    update_meters_for_ports(
        runtime, unit->output_ports, unit->output_ports_len, runtime->output_meters, runtime->output_meters_len, frames
    );
}

static void apply_project_mute(apg_v2_runtime_t *runtime, uint32_t frames) {
    if (!runtime || !runtime->project_muted || !runtime->plan || !runtime->plan->unit)
        return;
    const apg_unit_v2_t *unit = runtime->plan->unit;
    for (size_t i = 0; i < unit->output_ports_len; i++) {
        const apg_unit_v2_port_t *port = &unit->output_ports[i];
        if (!port || !port->type || strcmp(port->type, "audio") != 0)
            continue;
        size_t channels = 0;
        if (!parse_port_channel_count(port, &channels))
            continue;
        for (size_t ch = 0; ch < channels; ch++) {
            int index = signal_index_by_name(unit, audio_port_channel_signal_name(port, ch));
            if (index >= 0 && (size_t)index < runtime->signals_len && runtime->signals[index])
                memset(runtime->signals[index], 0, frames * sizeof(float));
        }
    }
}

bool apg_v2_runtime_set_param(apg_v2_runtime_t *runtime, const char *name, float value) {
    if (!runtime || !runtime->plan || !runtime->plan->unit || !name)
        return false;
    const apg_unit_v2_t *unit = runtime->plan->unit;
    for (size_t i = 0; i < unit->params_len && i < runtime->params_len; i++) {
        if (unit->params[i].name && strcmp(unit->params[i].name, name) == 0) {
            uint32_t smoothing_frames = runtime->has_processed ? param_smoothing_frames(&unit->params[i], runtime) : 0u;
            if (!runtime->param_targets || !runtime->param_smoothing_remaining_frames || smoothing_frames == 0u) {
                runtime->params[i] = value;
                if (runtime->param_targets)
                    runtime->param_targets[i] = value;
                if (runtime->param_smoothing_remaining_frames)
                    runtime->param_smoothing_remaining_frames[i] = 0u;
                return true;
            }
            runtime->param_targets[i]                    = value;
            runtime->param_smoothing_remaining_frames[i] = smoothing_frames;
            return true;
        }
    }
    return false;
}

bool apg_v2_runtime_set_control_port(apg_v2_runtime_t *runtime, const char *port_name, float value) {
    if (!runtime || !port_name)
        return false;
    for (size_t i = 0; i < runtime->control_targets_len; i++) {
        const apg_v2_runtime_control_target_t *target = &runtime->control_targets[i];
        if (!target->port_name || strcmp(target->port_name, port_name) != 0)
            continue;
        if (target->param_index >= runtime->params_len)
            return false;
        return apg_v2_runtime_set_param(runtime, target->param_name, value);
    }
    return false;
}

bool apg_v2_runtime_set_instance_bypass(apg_v2_runtime_t *runtime, const char *instance_id, bool enabled) {
    if (!runtime || !runtime->plan || !instance_id || instance_id[0] == '\0')
        return false;
    int index = runtime_instance_bypass_index(runtime, instance_id);
    if (!enabled) {
        if (index < 0)
            return true;
        free(runtime->bypassed_instances[index]);
        for (size_t i = (size_t)index + 1u; i < runtime->bypassed_instances_len; i++)
            runtime->bypassed_instances[i - 1u] = runtime->bypassed_instances[i];
        runtime->bypassed_instances_len--;
        return true;
    }
    if (index >= 0)
        return true;

    size_t input_index  = 0u;
    size_t output_index = 0u;
    if (!find_instance_bypass_io(runtime, instance_id, &input_index, &output_index)) {
        runtime_set_error(runtime, "v2 runtime cannot bypass unknown or unsupported instance");
        return false;
    }

    char **items = realloc(runtime->bypassed_instances, (runtime->bypassed_instances_len + 1u) * sizeof(*items));
    if (!items) {
        runtime_set_error(runtime, "v2 runtime bypass allocation failed");
        return false;
    }
    size_t len  = strlen(instance_id);
    char  *copy = malloc(len + 1u);
    if (!copy) {
        runtime->bypassed_instances = items;
        runtime_set_error(runtime, "v2 runtime bypass allocation failed");
        return false;
    }
    memcpy(copy, instance_id, len + 1u);
    runtime->bypassed_instances                                  = items;
    runtime->bypassed_instances[runtime->bypassed_instances_len] = copy;
    runtime->bypassed_instances_len++;
    return true;
}

bool apg_v2_runtime_set_project_mute(apg_v2_runtime_t *runtime, bool muted) {
    if (!runtime)
        return false;
    runtime->project_muted = muted;
    return true;
}

bool apg_v2_runtime_set_project_solo(apg_v2_runtime_t *runtime, bool soloed) {
    if (!runtime)
        return false;
    runtime->project_soloed = soloed;
    return true;
}

bool apg_v2_runtime_get_input_meter(
    const apg_v2_runtime_t *runtime, const char *port_name, size_t channel_index, apg_v2_meter_snapshot_t *out
) {
    return apg_v2_measure_get_input_meter(runtime, port_name, channel_index, out);
}

bool apg_v2_runtime_get_output_meter(
    const apg_v2_runtime_t *runtime, const char *port_name, size_t channel_index, apg_v2_meter_snapshot_t *out
) {
    return apg_v2_measure_get_output_meter(runtime, port_name, channel_index, out);
}

bool apg_v2_runtime_reset(apg_v2_runtime_t *runtime) {
    if (!runtime || !runtime->plan || !runtime->plan->unit)
        return false;
    runtime->last_error[0] = '\0';

    if (runtime->signal_pool && runtime->signals_len > 0u)
        memset(
            runtime->signal_pool, 0,
            runtime->signals_len * (size_t)runtime->frame_capacity * sizeof(*runtime->signal_pool)
        );
    if (runtime->input_meters && runtime->input_meters_len > 0u)
        memset(runtime->input_meters, 0, runtime->input_meters_len * sizeof(*runtime->input_meters));
    if (runtime->output_meters && runtime->output_meters_len > 0u)
        memset(runtime->output_meters, 0, runtime->output_meters_len * sizeof(*runtime->output_meters));
    for (size_t i = 0; i < runtime->params_len && i < runtime->plan->unit->params_len; i++) {
        runtime->params[i] = runtime->param_defaults ? runtime->param_defaults[i] : 0.0f;
        if (runtime->param_targets)
            runtime->param_targets[i] = runtime->params[i];
        if (runtime->param_smoothing_remaining_frames)
            runtime->param_smoothing_remaining_frames[i] = 0u;
    }
    runtime->has_processed = false;

    for (size_t i = 0; i < runtime->nodes_len && i < runtime->plan->nodes_len; i++) {
        apg_v2_runtime_node_t       *node = &runtime->nodes[i];
        const atom_registry_entry_t *atom = runtime->plan->nodes[i].atom;
        if (!node->state_storage || !atom)
            continue;
        memset(node->state_storage, 0, atom_storage_size(atom->state_size));

        size_t buffer_index = 0;
        for (int field_index = 0; field_index < atom->n_state_fields; field_index++) {
            const atom_field_desc_t *field = &atom->state_fields[field_index];
            if (field->type != FIELD_BUFFER)
                continue;
            if (buffer_index >= node->state_buffers_len || !node->state_buffers[buffer_index])
                return false;
            memset(node->state_buffers[buffer_index], 0, node->state_buffer_samples[buffer_index] * sizeof(float));
            float **field_ptr = (float **)((char *)node->state_storage + field->offset);
            *field_ptr        = node->state_buffers[buffer_index];
            buffer_index++;
        }
    }

    runtime->process_info.frames        = runtime->frame_capacity;
    runtime->process_info.output_frames = runtime->frame_capacity;
    runtime->process_info.channels      = 1u;
    return true;
}

bool apg_v2_runtime_process(apg_v2_runtime_t *runtime, uint32_t frames) {
    if (!runtime)
        return false;
    runtime->last_error[0] = '\0';
    if (!runtime->plan || !runtime->plan->unit) {
        runtime_set_error(runtime, "v2 runtime has no compiled plan");
        return false;
    }
    if (frames == 0u) {
        runtime_set_error(runtime, "v2 runtime frame count must be greater than zero");
        return false;
    }
    if (frames > runtime->frame_capacity) {
        runtime_set_error(runtime, "v2 runtime frame count exceeds capacity");
        return false;
    }

    runtime->process_info.frames        = frames;
    runtime->process_info.output_frames = frames;
    advance_smoothed_params(runtime, frames);

    uc_error err = {0};
    for (size_t i = 0; i < runtime->plan->schedule_len; i++) {
        uint32_t scheduled_index = runtime->plan->schedule[i];
        if (scheduled_index >= runtime->nodes_len) {
            runtime_set_error(runtime, "v2 runtime schedule index is out of range");
            return false;
        }
        const apg_v2_compiled_node_t *compiled     = &runtime->plan->nodes[scheduled_index];
        int                           bypass_index = runtime_node_bypass_index(runtime, compiled->id);
        if (bypass_index >= 0) {
            if (!apply_instance_bypass(runtime, runtime->bypassed_instances[bypass_index], frames))
                return false;
            continue;
        }
        if (refresh_node_config(compiled, runtime, &err) != UC_OK) {
            runtime_set_error(runtime, err.msg[0] ? err.msg : "v2 runtime config refresh failed");
            return false;
        }
        if (refresh_node_input_scalars(compiled, runtime, &err) != UC_OK) {
            runtime_set_error(runtime, err.msg[0] ? err.msg : "v2 runtime input refresh failed");
            return false;
        }
        compiled->atom->thunk(&runtime->nodes[scheduled_index].call);
    }
    apply_project_mute(runtime, frames);
    update_public_port_meters(runtime, frames);
    runtime->has_processed = true;
    return true;
}

bool apg_v2_runtime_process_interleaved_ports(
    apg_v2_runtime_t *runtime,
    const char       *input_port_name,
    const float      *input,
    const char       *output_port_name,
    float            *output,
    uint32_t          frames
) {
    if (!runtime)
        return false;
    runtime->last_error[0] = '\0';
    if (!runtime->plan || !runtime->plan->unit) {
        runtime_set_error(runtime, "v2 runtime has no compiled plan");
        return false;
    }
    if (!input || !output) {
        runtime_set_error(runtime, "v2 runtime interleaved input/output buffers are required");
        return false;
    }

    const apg_unit_v2_t      *unit = runtime->plan->unit;
    const apg_unit_v2_port_t *input_port =
        audio_port_by_name(unit->input_ports, unit->input_ports_len, input_port_name);
    const apg_unit_v2_port_t *output_port =
        audio_port_by_name(unit->output_ports, unit->output_ports_len, output_port_name);
    if (!input_port) {
        runtime_set_error(runtime, "v2 runtime input audio port signal lookup failed");
        return false;
    }
    if (!output_port) {
        runtime_set_error(runtime, "v2 runtime output audio port signal lookup failed");
        return false;
    }

    size_t input_channels  = 0;
    size_t output_channels = 0;
    if (!parse_port_channel_count(input_port, &input_channels) ||
        !parse_port_channel_count(output_port, &output_channels)) {
        runtime_set_error(runtime, "v2 runtime audio port channel count is invalid");
        return false;
    }

    for (size_t ch = 0; ch < input_channels; ch++) {
        int index = audio_port_signal_index_by_channel_name(
            unit, unit->input_ports, unit->input_ports_len, input_port_name, ch
        );
        if (index < 0 || (size_t)index >= runtime->signals_len) {
            runtime_set_error(runtime, "v2 runtime input audio port signal lookup failed");
            return false;
        }
    }
    for (size_t ch = 0; ch < output_channels; ch++) {
        int index = audio_port_signal_index_by_channel_name(
            unit, unit->output_ports, unit->output_ports_len, output_port_name, ch
        );
        if (index < 0 || (size_t)index >= runtime->signals_len) {
            runtime_set_error(runtime, "v2 runtime output audio port signal lookup failed");
            return false;
        }
    }

    if (frames > runtime->frame_capacity || frames == 0u)
        return apg_v2_runtime_process(runtime, frames);

    for (size_t ch = 0; ch < input_channels; ch++) {
        int index = audio_port_signal_index_by_channel_name(
            unit, unit->input_ports, unit->input_ports_len, input_port_name, ch
        );
        for (uint32_t frame = 0; frame < frames; frame++)
            runtime->signals[index][frame] = input[(size_t)frame * input_channels + ch];
    }

    runtime->process_info.channels = (uint32_t)output_channels;
    if (!apg_v2_runtime_process(runtime, frames))
        return false;

    for (size_t ch = 0; ch < output_channels; ch++) {
        int index = audio_port_signal_index_by_channel_name(
            unit, unit->output_ports, unit->output_ports_len, output_port_name, ch
        );
        for (uint32_t frame = 0; frame < frames; frame++)
            output[(size_t)frame * output_channels + ch] = runtime->signals[index][frame];
    }
    return true;
}

bool apg_v2_runtime_process_mono_ports(
    apg_v2_runtime_t *runtime,
    const char       *input_port_name,
    const float      *input,
    const char       *output_port_name,
    float            *output,
    uint32_t          frames
) {
    if (!runtime)
        return false;
    runtime->last_error[0] = '\0';
    if (!runtime->plan || !runtime->plan->unit) {
        runtime_set_error(runtime, "v2 runtime has no compiled plan");
        return false;
    }
    if (!input || !output) {
        runtime_set_error(runtime, "v2 runtime mono input/output buffers are required");
        return false;
    }

    const apg_unit_v2_t      *unit = runtime->plan->unit;
    const apg_unit_v2_port_t *input_port =
        audio_port_by_name(unit->input_ports, unit->input_ports_len, input_port_name);
    const apg_unit_v2_port_t *output_port =
        audio_port_by_name(unit->output_ports, unit->output_ports_len, output_port_name);
    int input_index = audio_port_signal_index_by_name(unit, unit->input_ports, unit->input_ports_len, input_port_name);
    int output_index =
        audio_port_signal_index_by_name(unit, unit->output_ports, unit->output_ports_len, output_port_name);
    if (!input_port || input_index < 0 || (size_t)input_index >= runtime->signals_len) {
        runtime_set_error(runtime, "v2 runtime input audio port signal lookup failed");
        return false;
    }
    if (!output_port || output_index < 0 || (size_t)output_index >= runtime->signals_len) {
        runtime_set_error(runtime, "v2 runtime output audio port signal lookup failed");
        return false;
    }
    if (!input_port->channels || strcmp(input_port->channels, "1") != 0 || !output_port->channels ||
        strcmp(output_port->channels, "1") != 0) {
        runtime_set_error(runtime, "v2 runtime mono processing requires mono audio ports");
        return false;
    }

    if (frames > runtime->frame_capacity || frames == 0u)
        return apg_v2_runtime_process(runtime, frames);
    memcpy(runtime->signals[input_index], input, frames * sizeof(float));
    if (!apg_v2_runtime_process(runtime, frames))
        return false;
    memcpy(output, runtime->signals[output_index], frames * sizeof(float));
    return true;
}

bool apg_v2_runtime_process_mono(apg_v2_runtime_t *runtime, const float *input, float *output, uint32_t frames) {
    if (!runtime || !runtime->plan || !runtime->plan->unit)
        return apg_v2_runtime_process_mono_ports(runtime, NULL, input, NULL, output, frames);

    const apg_unit_v2_t *unit = runtime->plan->unit;
    return apg_v2_runtime_process_mono_ports(
        runtime, first_audio_port_name(unit->input_ports, unit->input_ports_len), input,
        first_audio_port_name(unit->output_ports, unit->output_ports_len), output, frames
    );
}

const char *apg_v2_runtime_last_error(const apg_v2_runtime_t *runtime) { return apg_v2_measure_last_error(runtime); }

void apg_v2_runtime_destroy(apg_v2_runtime_t *runtime) {
    if (!runtime)
        return;

    for (size_t i = 0; i < runtime->nodes_len; i++) {
        for (size_t j = 0; j < runtime->nodes[i].state_buffers_len; j++)
            free(runtime->nodes[i].state_buffers[j]);
        free(runtime->nodes[i].state_buffers);
        free(runtime->nodes[i].state_buffer_samples);
        free(runtime->nodes[i].signal_array_pool);
        for (size_t j = 0; j < runtime->nodes[i].aux_blocks_len; j++)
            free(runtime->nodes[i].aux_blocks[j]);
        free(runtime->nodes[i].aux_blocks);
        free(runtime->nodes[i].out_storage);
        free(runtime->nodes[i].in_storage);
        free(runtime->nodes[i].config_storage);
        free(runtime->nodes[i].state_storage);
    }
    free(runtime->nodes);
    for (size_t i = 0; i < runtime->bypassed_instances_len; i++)
        free(runtime->bypassed_instances[i]);
    free(runtime->bypassed_instances);
    free(runtime->control_targets);
    free(runtime->params);
    free(runtime->param_defaults);
    free(runtime->param_targets);
    free(runtime->param_smoothing_remaining_frames);
    free(runtime->input_meters);
    free(runtime->output_meters);
    free(runtime->signals);
    free(runtime->signal_pool);
    memset(runtime, 0, sizeof(*runtime));
}
