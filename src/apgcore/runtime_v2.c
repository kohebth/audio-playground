#include <apgcore/runtime_v2.h>
#include <atom/dsp_types.h>

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const size_t INVALID_BYPASS_INDEX = (size_t)-1u;

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

static const apg_v2_runtime_audio_port_t *
runtime_audio_port_by_name(const apg_v2_runtime_audio_port_t *ports, size_t ports_len, const char *port_name) {
    if (!ports || !port_name)
        return NULL;
    for (size_t i = 0; i < ports_len; i++) {
        if (ports[i].port_name && strcmp(ports[i].port_name, port_name) == 0)
            return &ports[i];
    }
    return NULL;
}

static bool
runtime_bypass_entry_matches_instance_id(const apg_v2_runtime_bypass_entry_t *entry, const char *instance_id) {
    if (!entry || !entry->instance_id || !instance_id || entry->instance_id_len == 0u)
        return false;
    size_t instance_len = strlen(instance_id);
    if (instance_len != entry->instance_id_len)
        return false;
    return strncmp(entry->instance_id, instance_id, instance_len) == 0;
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

static uc_status
init_bypass_state_from_image(const apg_v2_runtime_image_t *image, apg_v2_runtime_t *runtime, uc_error *err) {
    if (!image || !runtime)
        return UC_E_TYPE;

    runtime->bypassed_instances_len = image->bypassed_instances_len;
    if (runtime->bypassed_instances_len == 0u)
        return UC_OK;

    runtime->bypassed_instances = calloc(runtime->bypassed_instances_len, sizeof(*runtime->bypassed_instances));
    if (!runtime->bypassed_instances)
        return set_error(err, UC_E_OOM, "v2 runtime bypass instance allocation failed");

    for (size_t i = 0; i < runtime->bypassed_instances_len; i++) {
        runtime->bypassed_instances[i] = (apg_v2_runtime_bypass_entry_t){
            .instance_id     = image->bypass_instances[i].instance_id,
            .instance_id_len = image->bypass_instances[i].instance_id_len,
            .input_index     = image->bypass_instances[i].input_index,
            .output_index    = image->bypass_instances[i].output_index,
            .enabled         = false,
        };
    }
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

static float compiled_scalar_value(const apg_v2_compiled_binding_t *binding, const apg_v2_runtime_t *runtime) {
    if (binding->kind == APG_BIND_PARAM)
        return binding->index < runtime->params_len ? runtime->params[binding->index] : 0.0f;
    if (binding->kind == APG_BIND_LITERAL)
        return binding->literal ? strtof(binding->literal, NULL) : 0.0f;
    return 0.0f;
}

static uc_status apply_signal_bindings(
    const apg_v2_runtime_node_layout_t *layout, apg_v2_runtime_t *runtime, apg_v2_runtime_node_t *node, uc_error *err
) {
    if (!layout || !runtime || !node)
        return UC_OK;

    for (size_t i = 0; i < layout->signal_bindings_len; i++) {
        const apg_v2_runtime_signal_binding_t *entry   = &layout->signal_bindings[i];
        void                                  *storage = entry->is_input ? node->in_storage : node->out_storage;
        if (!entry->binding || !entry->binding->key)
            return set_error(err, UC_E_MISSING, "v2 runtime signal binding metadata is missing");

        if (entry->is_signal_array) {
            if (entry->signal_array_len == 0u)
                return set_error(err, UC_E_RANGE, "v2 runtime signal binding has empty signal array");
            if (entry->signal_array_offset + entry->signal_array_len > node->signal_array_pool_len)
                return set_error(err, UC_E_RANGE, "v2 runtime signal array pool is too small");

            float **signal_array = &node->signal_array_pool[entry->signal_array_offset];
            for (size_t j = 0; j < entry->signal_array_len; j++) {
                size_t signal_index = entry->binding->indices[j];
                if (signal_index >= runtime->signals_len)
                    return set_error(err, UC_E_MISSING, "v2 runtime signal binding references invalid signal index");
                signal_array[j] = runtime->signals[signal_index];
            }
            *(float ***)(((char *)storage) + entry->storage_offset) = signal_array;
            continue;
        }

        if (entry->signal_index >= runtime->signals_len)
            return set_error(err, UC_E_MISSING, "v2 runtime signal binding references invalid signal index");
        *(float **)(((char *)storage) + entry->storage_offset) = runtime->signals[entry->signal_index];
    }
    return UC_OK;
}

static uc_status refresh_scalar_plan(
    const apg_v2_runtime_scalar_refresh_t *items,
    size_t                                 items_len,
    const apg_v2_compiled_node_t          *compiled_node,
    apg_v2_runtime_t                      *runtime,
    void                                  *storage,
    uc_error                              *err
) {
    if (!runtime || !runtime->plan || !runtime->plan->unit || !compiled_node || !compiled_node->atom)
        return set_error(err, UC_E_MISSING, "v2 runtime scalar refresh context is missing");

    for (size_t i = 0; i < items_len; i++) {
        if (!items[i].binding)
            return set_error(err, UC_E_MISSING, "v2 runtime scalar refresh metadata is missing");
        if (items[i].binding_key && items[i].binding->key && strcmp(items[i].binding_key, items[i].binding->key) != 0) {
            char msg[192];
            snprintf(
                msg, sizeof(msg), "node '%s' atom '%s' %s binding key '%s' metadata is missing",
                compiled_node->id ? compiled_node->id : "", compiled_node->atom->name ? compiled_node->atom->name : "",
                items[i].config ? "config" : "input", items[i].binding->key
            );
            return set_error(err, UC_E_MISSING, msg);
        }
        if (items[i].binding->kind != APG_BIND_PARAM && items[i].binding->kind != APG_BIND_LITERAL) {
            char msg[192];
            snprintf(
                msg, sizeof(msg), "node '%s' atom '%s' %s binding key '%s' metadata is missing",
                compiled_node->id ? compiled_node->id : "", compiled_node->atom->name ? compiled_node->atom->name : "",
                items[i].config ? "config" : "input", items[i].binding->key ? items[i].binding->key : ""
            );
            return set_error(err, UC_E_TYPE, msg);
        }
        if (items[i].binding->kind == APG_BIND_PARAM) {
            uint32_t param_index = items[i].binding->index;
            if (param_index >= runtime->params_len) {
                return set_error(err, UC_E_RANGE, "v2 runtime scalar refresh param index is out of bounds");
            }
        }
        void *addr  = (char *)storage + items[i].storage_offset;
        float value = compiled_scalar_value(items[i].binding, runtime);
        if (items[i].field_type == FIELD_INT)
            *(int *)addr = (int)value;
        else if (items[i].field_type == FIELD_FLOAT)
            *(float *)addr = value;
        else {
            return set_error(err, UC_E_TYPE, "v2 runtime scalar refresh field type is unsupported");
        }
    }
    return UC_OK;
}

static uc_status
apply_mix_matrix_config(const apg_v2_runtime_node_layout_t *layout, apg_v2_runtime_node_t *node, uc_error *err) {
    if (!layout || !node)
        return UC_OK;
    if (!layout->mix_matrix_row_pointers)
        return UC_OK;
    if (layout->mix_matrix_num_out > INT_MAX || layout->mix_matrix_num_in > INT_MAX)
        return set_error(err, UC_E_RANGE, "v2 runtime mix_matrix row/col counts exceed int range");

    mix_matrix_params_t *params = (mix_matrix_params_t *)node->config_storage;
    params->coefficients        = layout->mix_matrix_row_pointers;
    params->num_out             = (int)layout->mix_matrix_num_out;
    params->num_in              = (int)layout->mix_matrix_num_in;
    return UC_OK;
}

static uc_status init_state_buffers(
    const apg_v2_compiled_node_t       *compiled,
    const apg_v2_runtime_node_layout_t *layout,
    apg_v2_runtime_t                   *runtime,
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
        size_t offset = layout->state_buffer_sample_offsets_by_index
                            ? layout->state_buffer_sample_offsets_by_index[buffer_index]
                            : 0u;
        if (!runtime || !runtime->state_buffer_pool || offset + buffer_samples > runtime->state_buffer_samples)
            return set_error(err, UC_E_RANGE, "v2 runtime state buffer layout exceeds pool");
        float *buffer                            = &runtime->state_buffer_pool[offset];
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
    out->atom_storage_bytes = image->atom_storage_bytes;
    if (out->atom_storage_bytes > 0u) {
        out->atom_storage_pool = calloc(1u, out->atom_storage_bytes);
        if (!out->atom_storage_pool)
            return set_error(err, UC_E_OOM, "v2 runtime atom storage pool allocation failed");
    }
    out->state_buffer_samples = image->state_buffer_samples;
    if (out->state_buffer_samples > 0u) {
        out->state_buffer_pool = calloc(out->state_buffer_samples, sizeof(*out->state_buffer_pool));
        if (!out->state_buffer_pool)
            return set_error(err, UC_E_OOM, "v2 runtime state buffer pool allocation failed");
    }
    if (image->signal_array_pointer_slots > 0u) {
        out->signal_array_pool_len = image->signal_array_pointer_slots;
        out->signal_array_pool     = calloc(out->signal_array_pool_len, sizeof(*out->signal_array_pool));
        if (!out->signal_array_pool)
            return set_error(err, UC_E_OOM, "v2 runtime signal array pool allocation failed");
    }

    for (size_t i = 0; i < out->nodes_len; i++) {
        const atom_registry_entry_t        *atom   = plan->nodes[i].atom;
        apg_v2_runtime_node_t              *node   = &out->nodes[i];
        const apg_v2_runtime_node_layout_t *layout = &image->node_layouts[i];
        if (!atom)
            return set_error(err, UC_E_MISSING, "v2 runtime node is missing atom metadata");

        if (layout->out_offset + layout->out_size > out->atom_storage_bytes ||
            layout->in_offset + layout->in_size > out->atom_storage_bytes ||
            layout->config_offset + layout->config_size > out->atom_storage_bytes ||
            layout->state_offset + layout->state_size > out->atom_storage_bytes)
            return set_error(err, UC_E_RANGE, "v2 runtime image atom storage layout exceeds pool");
        node->out_storage            = (char *)out->atom_storage_pool + layout->out_offset;
        node->in_storage             = (char *)out->atom_storage_pool + layout->in_offset;
        node->config_storage         = (char *)out->atom_storage_pool + layout->config_offset;
        node->state_storage          = (char *)out->atom_storage_pool + layout->state_offset;
        node->signal_array_pool      = out->signal_array_pool + layout->signal_array_pool_offset;
        node->signal_array_pool_len  = layout->signal_array_pointer_slots;
        node->signal_array_pool_used = layout->signal_array_pointer_slots;
        node->config_refreshes       = layout->config_refreshes;
        node->config_refreshes_len   = layout->config_refreshes_len;
        node->input_refreshes        = layout->input_refreshes;
        node->input_refreshes_len    = layout->input_refreshes_len;

        node->call.out    = node->out_storage;
        node->call.in     = node->in_storage;
        node->call.config = node->config_storage;
        node->call.state  = node->state_storage;
        node->call.info   = &out->process_info;

        uc_status status = init_state_buffers(&plan->nodes[i], layout, out, node, err);
        if (status != UC_OK)
            return status;
        node->signal_bindings     = layout->signal_bindings;
        node->signal_bindings_len = layout->signal_bindings_len;
        status                    = apply_signal_bindings(layout, out, node, err);
        if (status != UC_OK)
            return status;
        status = refresh_scalar_plan(
            node->config_refreshes, node->config_refreshes_len, &plan->nodes[i], out, node->config_storage, err
        );
        if (status != UC_OK)
            return status;
        status = refresh_scalar_plan(
            node->input_refreshes, node->input_refreshes_len, &plan->nodes[i], out, node->in_storage, err
        );
        if (status != UC_OK)
            return status;
        status = apply_mix_matrix_config(layout, node, err);
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
    out->input_meters_len       = image->input_meters_len;
    out->output_meters_len      = image->output_meters_len;
    out->input_audio_ports      = image->input_audio_ports;
    out->input_audio_ports_len  = image->input_audio_ports_len;
    out->output_audio_ports     = image->output_audio_ports;
    out->output_audio_ports_len = image->output_audio_ports_len;
    status                      = init_params(image, out, err);
    if (status != UC_OK)
        goto fail;
    out->control_targets     = image->control_targets;
    out->control_targets_len = image->control_targets_len;
    status                   = init_node_calls(image, out, err);
    if (status != UC_OK)
        goto fail;
    status = init_bypass_state_from_image(image, out, err);
    if (status != UC_OK)
        goto fail;
    out->bypass_index_by_node            = image->bypass_index_by_node;
    out->project_mute_output_indices     = image->project_mute_output_indices;
    out->project_mute_output_indices_len = image->project_mute_output_indices_len;
    if (out->nodes_len != 0u && image->bypassed_instances_len > 0u && !out->bypass_index_by_node) {
        status = set_error(err, UC_E_MISSING, "v2 runtime bypass index map is missing from runtime image");
        goto fail;
    }
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

    memset(out, 0, sizeof(*out));
    uc_status status = apg_v2_runtime_init_from_plan(plan, frame_capacity, sample_rate, &out->image_arena, out, err);
    if (status != UC_OK)
        return status;

    return UC_OK;
}

uc_status apg_v2_runtime_init_from_plan(
    const apg_v2_compiled_unit_t *plan,
    uint32_t                      frame_capacity,
    float                         sample_rate,
    uc_arena                     *image_arena,
    apg_v2_runtime_t             *out,
    uc_error                     *err
) {
    if (!plan || !plan->unit || !out || !err || !image_arena)
        return UC_E_TYPE;

    apg_v2_runtime_image_t image = {0};
    uc_status              status =
        apg_v2_runtime_image_build_with_growth(plan, frame_capacity, sample_rate, image_arena, &image, err);
    if (status != UC_OK)
        return status;

    status = apg_v2_runtime_init_from_image(&image, out, err);
    if (status != UC_OK) {
        apg_v2_runtime_destroy(out);
        uc_arena_free(image_arena);
        *image_arena = (uc_arena){0};
        return status;
    }

    out->image_arena       = *image_arena;
    out->image_arena_ready = true;
    return UC_OK;
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
    if (!runtime || !port_name)
        return NULL;
    const apg_v2_runtime_audio_port_t *port =
        runtime_audio_port_by_name(runtime->input_audio_ports, runtime->input_audio_ports_len, port_name);
    if (!port || port->channel_count == 0u || port->signal_indices[0] >= runtime->signals_len)
        return NULL;
    return runtime->signals[port->signal_indices[0]];
}

float *apg_v2_runtime_find_output_port_signal(apg_v2_runtime_t *runtime, const char *port_name) {
    if (!runtime || !port_name)
        return NULL;
    const apg_v2_runtime_audio_port_t *port =
        runtime_audio_port_by_name(runtime->output_audio_ports, runtime->output_audio_ports_len, port_name);
    if (!port || port->channel_count == 0u || port->signal_indices[0] >= runtime->signals_len)
        return NULL;
    return runtime->signals[port->signal_indices[0]];
}

float *
apg_v2_runtime_find_input_port_channel_signal(apg_v2_runtime_t *runtime, const char *port_name, size_t channel_index) {
    if (!runtime || !port_name)
        return NULL;
    const apg_v2_runtime_audio_port_t *port =
        runtime_audio_port_by_name(runtime->input_audio_ports, runtime->input_audio_ports_len, port_name);
    if (!port || channel_index >= port->channel_count || port->signal_indices[channel_index] >= runtime->signals_len)
        return NULL;
    return runtime->signals[port->signal_indices[channel_index]];
}

float *
apg_v2_runtime_find_output_port_channel_signal(apg_v2_runtime_t *runtime, const char *port_name, size_t channel_index) {
    if (!runtime || !port_name)
        return NULL;
    const apg_v2_runtime_audio_port_t *port =
        runtime_audio_port_by_name(runtime->output_audio_ports, runtime->output_audio_ports_len, port_name);
    if (!port || channel_index >= port->channel_count || port->signal_indices[channel_index] >= runtime->signals_len)
        return NULL;
    return runtime->signals[port->signal_indices[channel_index]];
}

static size_t runtime_instance_bypass_index(const apg_v2_runtime_t *runtime, const char *instance_id) {
    if (!runtime || !instance_id || !runtime->bypassed_instances)
        return INVALID_BYPASS_INDEX;
    for (size_t i = 0; i < runtime->bypassed_instances_len; i++) {
        if (runtime_bypass_entry_matches_instance_id(&runtime->bypassed_instances[i], instance_id))
            return i;
    }
    return INVALID_BYPASS_INDEX;
}

static size_t runtime_node_bypass_index(const apg_v2_runtime_t *runtime, size_t node_index) {
    if (!runtime || !runtime->bypass_index_by_node || node_index >= runtime->nodes_len)
        return INVALID_BYPASS_INDEX;
    return runtime->bypass_index_by_node[node_index];
}

static bool
apply_instance_bypass(apg_v2_runtime_t *runtime, const apg_v2_runtime_bypass_entry_t *entry, uint32_t frames) {
    if (!runtime || !entry)
        return false;
    if (entry->input_index >= runtime->signals_len || entry->output_index >= runtime->signals_len)
        return false;
    if (!runtime->signals[entry->input_index] || !runtime->signals[entry->output_index]) {
        runtime_set_error(runtime, "v2 runtime instance bypass signal lookup failed");
        return false;
    }
    memcpy(runtime->signals[entry->output_index], runtime->signals[entry->input_index], frames * sizeof(float));
    return true;
}

static void apply_project_mute(apg_v2_runtime_t *runtime, uint32_t frames) {
    if (!runtime || !runtime->project_muted || runtime->project_mute_output_indices_len == 0u)
        return;
    for (size_t i = 0; i < runtime->project_mute_output_indices_len; i++) {
        size_t index = runtime->project_mute_output_indices[i];
        if (index < runtime->signals_len && runtime->signals[index])
            memset(runtime->signals[index], 0, frames * sizeof(float));
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
    size_t index = runtime_instance_bypass_index(runtime, instance_id);
    if (!enabled) {
        if (index == INVALID_BYPASS_INDEX)
            return true;
        runtime->bypassed_instances[index].enabled = false;
        return true;
    }
    if (index != INVALID_BYPASS_INDEX) {
        runtime->bypassed_instances[index].enabled = true;
        return true;
    }

    runtime_set_error(runtime, "v2 runtime cannot bypass unknown or unsupported instance");
    return false;
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

bool apg_v2_runtime_reset(apg_v2_runtime_t *runtime) {
    if (!runtime || !runtime->plan || !runtime->plan->unit)
        return false;
    runtime->last_error[0] = '\0';

    if (runtime->signal_pool && runtime->signals_len > 0u)
        memset(
            runtime->signal_pool, 0,
            runtime->signals_len * (size_t)runtime->frame_capacity * sizeof(*runtime->signal_pool)
        );
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
        size_t                        bypass_index = runtime_node_bypass_index(runtime, scheduled_index);
        if (bypass_index != INVALID_BYPASS_INDEX && runtime->bypassed_instances[bypass_index].enabled) {
            if (!apply_instance_bypass(runtime, &runtime->bypassed_instances[bypass_index], frames))
                return false;
            continue;
        }
        apg_v2_runtime_node_t *node = &runtime->nodes[scheduled_index];
        if (refresh_scalar_plan(
                node->config_refreshes, node->config_refreshes_len, compiled, runtime, node->config_storage, &err
            ) != UC_OK) {
            runtime_set_error(runtime, err.msg[0] ? err.msg : "v2 runtime config refresh failed");
            return false;
        }
        if (refresh_scalar_plan(
                node->input_refreshes, node->input_refreshes_len, compiled, runtime, node->in_storage, &err
            ) != UC_OK) {
            runtime_set_error(runtime, err.msg[0] ? err.msg : "v2 runtime input refresh failed");
            return false;
        }
        compiled->atom->thunk(&runtime->nodes[scheduled_index].call);
    }
    apply_project_mute(runtime, frames);
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

    const apg_v2_runtime_audio_port_t *input_port =
        runtime_audio_port_by_name(runtime->input_audio_ports, runtime->input_audio_ports_len, input_port_name);
    const apg_v2_runtime_audio_port_t *output_port =
        runtime_audio_port_by_name(runtime->output_audio_ports, runtime->output_audio_ports_len, output_port_name);
    if (!input_port) {
        runtime_set_error(runtime, "v2 runtime input audio port signal lookup failed");
        return false;
    }
    if (!output_port) {
        runtime_set_error(runtime, "v2 runtime output audio port signal lookup failed");
        return false;
    }

    size_t input_channels  = input_port->channel_count;
    size_t output_channels = output_port->channel_count;
    for (size_t ch = 0; ch < input_channels; ch++) {
        if (input_port->signal_indices[ch] >= runtime->signals_len) {
            runtime_set_error(runtime, "v2 runtime input audio port signal lookup failed");
            return false;
        }
    }
    for (size_t ch = 0; ch < output_channels; ch++) {
        if (output_port->signal_indices[ch] >= runtime->signals_len) {
            runtime_set_error(runtime, "v2 runtime output audio port signal lookup failed");
            return false;
        }
    }

    if (frames > runtime->frame_capacity || frames == 0u)
        return apg_v2_runtime_process(runtime, frames);

    for (size_t ch = 0; ch < input_channels; ch++) {
        size_t index = input_port->signal_indices[ch];
        for (uint32_t frame = 0; frame < frames; frame++)
            runtime->signals[index][frame] = input[(size_t)frame * input_channels + ch];
    }

    runtime->process_info.channels = (uint32_t)output_channels;
    if (!apg_v2_runtime_process(runtime, frames))
        return false;

    for (size_t ch = 0; ch < output_channels; ch++) {
        size_t index = output_port->signal_indices[ch];
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

    const apg_v2_runtime_audio_port_t *input_port =
        runtime_audio_port_by_name(runtime->input_audio_ports, runtime->input_audio_ports_len, input_port_name);
    const apg_v2_runtime_audio_port_t *output_port =
        runtime_audio_port_by_name(runtime->output_audio_ports, runtime->output_audio_ports_len, output_port_name);
    if (!input_port || input_port->channel_count == 0u || input_port->signal_indices[0] >= runtime->signals_len) {
        runtime_set_error(runtime, "v2 runtime input audio port signal lookup failed");
        return false;
    }
    if (!output_port || output_port->channel_count == 0u || output_port->signal_indices[0] >= runtime->signals_len) {
        runtime_set_error(runtime, "v2 runtime output audio port signal lookup failed");
        return false;
    }
    if (input_port->channel_count != 1u || output_port->channel_count != 1u) {
        runtime_set_error(runtime, "v2 runtime mono processing requires mono audio ports");
        return false;
    }

    if (frames > runtime->frame_capacity || frames == 0u)
        return apg_v2_runtime_process(runtime, frames);
    memcpy(runtime->signals[input_port->signal_indices[0]], input, frames * sizeof(float));
    if (!apg_v2_runtime_process(runtime, frames))
        return false;
    memcpy(output, runtime->signals[output_port->signal_indices[0]], frames * sizeof(float));
    return true;
}

bool apg_v2_runtime_process_mono(apg_v2_runtime_t *runtime, const float *input, float *output, uint32_t frames) {
    if (!runtime)
        return apg_v2_runtime_process_mono_ports(runtime, NULL, input, NULL, output, frames);

    return apg_v2_runtime_process_mono_ports(
        runtime, runtime->input_audio_ports_len > 0u ? runtime->input_audio_ports[0].port_name : NULL, input,
        runtime->output_audio_ports_len > 0u ? runtime->output_audio_ports[0].port_name : NULL, output, frames
    );
}

void apg_v2_runtime_destroy(apg_v2_runtime_t *runtime) {
    if (!runtime)
        return;

    if (runtime->image_arena_ready)
        uc_arena_free(&runtime->image_arena);

    for (size_t i = 0; i < runtime->nodes_len; i++) {
        free(runtime->nodes[i].state_buffers);
        free(runtime->nodes[i].state_buffer_samples);
    }
    free(runtime->signal_array_pool);
    free(runtime->nodes);
    free(runtime->atom_storage_pool);
    free(runtime->state_buffer_pool);
    free(runtime->bypassed_instances);
    free(runtime->params);
    free(runtime->param_defaults);
    free(runtime->param_targets);
    free(runtime->param_smoothing_remaining_frames);
    free(runtime->signals);
    free(runtime->signal_pool);
    memset(runtime, 0, sizeof(*runtime));
}
