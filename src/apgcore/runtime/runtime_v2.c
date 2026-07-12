#include <apgcore/runtime/runtime_v2_internal.h>
#include <atom/dsp_types.h>

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const size_t INVALID_BYPASS_INDEX = (size_t)-1u;

typedef enum {
    APG_RUNTIME_RESET_BEGIN,
    APG_RUNTIME_RESET_CLEAR_SIGNALS,
    APG_RUNTIME_RESET_RESTORE_PARAMS,
    APG_RUNTIME_RESET_RESET_NODE,
    APG_RUNTIME_RESET_RESET_PROCESS_INFO,
    APG_RUNTIME_RESET_DONE,
    APG_RUNTIME_RESET_FAIL,
} apg_runtime_reset_state_t;

typedef struct {
    apg_v2_runtime_t         *runtime;
    apg_runtime_reset_state_t state;
    size_t                    node_cursor;
} apg_runtime_reset_context_t;

static uc_status set_error(uc_error *err, uc_status status, const char *msg) {
    uc_loc loc = {0, 0};
    uc_error_set(err, status, loc, "%s", msg);
    return status;
}

static size_t atom_storage_size(size_t size) { return size > 0u ? size : 1u; }

void apg_v2_runtime_set_error(apg_v2_runtime_t *runtime, const char *msg) {
    if (!runtime || !msg)
        return;
    snprintf(runtime->last_error, sizeof(runtime->last_error), "%s", msg);
}

bool apg_v2_runtime_execution_metadata_ready(const apg_v2_runtime_t *runtime) {
    return runtime && (runtime->schedule || runtime->schedule_len == 0u) &&
           (runtime->nodes || runtime->nodes_len == 0u);
}

static uc_status init_signal_buffers(const apg_v2_registry_t *registry, apg_v2_runtime_t *out, uc_error *err) {
    out->signals_len = registry->signals_len;
    if (out->signals_len == 0u)
        return UC_OK;
    if (registry->signal_samples > SIZE_MAX / sizeof(float))
        return set_error(err, UC_E_RANGE, "v2 runtime signal pool is too large");

    out->signals     = calloc(out->signals_len, sizeof(*out->signals));
    out->signal_pool = calloc(registry->signal_samples, sizeof(*out->signal_pool));
    if (!out->signals || !out->signal_pool)
        return set_error(err, UC_E_OOM, "v2 runtime signal allocation failed");

    for (size_t i = 0; i < out->signals_len; i++)
        out->signals[i] = &out->signal_pool[i * (size_t)registry->frame_capacity];
    return UC_OK;
}

static uc_status init_params(const apg_v2_registry_t *registry, apg_v2_runtime_t *out, uc_error *err) {
    out->params_len = registry->params_len;
    if (out->params_len == 0u)
        return UC_OK;

    out->params                           = calloc(out->params_len, sizeof(*out->params));
    out->param_defaults                   = calloc(out->params_len, sizeof(*out->param_defaults));
    out->param_targets                    = calloc(out->params_len, sizeof(*out->param_targets));
    out->param_smoothing_remaining_frames = calloc(out->params_len, sizeof(*out->param_smoothing_remaining_frames));
    if (!out->params || !out->param_defaults || !out->param_targets || !out->param_smoothing_remaining_frames)
        return set_error(err, UC_E_OOM, "v2 runtime param allocation failed");

    for (size_t i = 0; i < out->params_len; i++) {
        out->param_defaults[i] = registry->param_defaults ? registry->param_defaults[i] : 0.0f;
        out->params[i]         = out->param_defaults[i];
        out->param_targets[i]  = out->params[i];
    }
    out->param_smoothing_frames = registry->param_smoothing_frames;
    return UC_OK;
}

static uc_status
init_bypass_state_from_registry(const apg_v2_registry_t *registry, apg_v2_runtime_t *runtime, uc_error *err) {
    if (!registry || !runtime)
        return UC_E_TYPE;

    runtime->bypassed_instances_len = registry->bypassed_instances_len;
    if (runtime->bypassed_instances_len == 0u)
        return UC_OK;

    runtime->bypassed_instances = calloc(runtime->bypassed_instances_len, sizeof(*runtime->bypassed_instances));
    if (!runtime->bypassed_instances)
        return set_error(err, UC_E_OOM, "v2 runtime bypass instance allocation failed");

    for (size_t i = 0; i < runtime->bypassed_instances_len; i++) {
        runtime->bypassed_instances[i] = (apg_v2_runtime_bypass_entry_t){
            .instance_id     = registry->bypass_instances[i].instance_id,
            .instance_id_len = registry->bypass_instances[i].instance_id_len,
            .input_index     = registry->bypass_instances[i].input_index,
            .output_index    = registry->bypass_instances[i].output_index,
            .enabled         = false,
        };
    }
    return UC_OK;
}

void apg_v2_runtime_advance_smoothed_params(apg_v2_runtime_t *runtime, uint32_t frames) {
    if (!runtime || !runtime->params || !runtime->param_targets || !runtime->param_smoothing_remaining_frames)
        return;
    for (size_t i = 0; i < runtime->params_len; i++) {
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

static float scalar_refresh_value(const apg_v2_registry_scalar_refresh_t *item, const apg_v2_runtime_t *runtime) {
    if (item->kind == APG_BIND_PARAM)
        return item->param_index < runtime->params_len ? runtime->params[item->param_index] : 0.0f;
    if (item->kind == APG_BIND_LITERAL)
        return item->number;
    return 0.0f;
}

static bool refresh_scalar_plan_runtime(
    const apg_v2_registry_scalar_refresh_t *items,
    size_t                                  items_len,
    const char                             *node_id,
    const char                             *atom_name,
    apg_v2_runtime_t                       *runtime,
    void                                   *storage
);

static uc_status apply_signal_bindings(
    const apg_v2_registry_node_layout_t *layout, apg_v2_runtime_t *runtime, apg_v2_runtime_node_t *node, uc_error *err
) {
    if (!layout || !runtime || !node)
        return UC_OK;

    for (size_t i = 0; i < layout->signal_bindings_len; i++) {
        const apg_v2_registry_signal_binding_t *entry   = &layout->signal_bindings[i];
        void                                   *storage = entry->is_input ? node->in_storage : node->out_storage;

        if (entry->is_signal_array) {
            if (entry->signal_array_len == 0u)
                return set_error(err, UC_E_RANGE, "v2 runtime signal binding has empty signal array");
            if (!entry->signal_array_indices)
                return set_error(err, UC_E_MISSING, "v2 runtime signal array index metadata is missing");
            if (entry->signal_array_offset + entry->signal_array_len > node->signal_array_pool_len)
                return set_error(err, UC_E_RANGE, "v2 runtime signal array pool is too small");

            float **signal_array = &node->signal_array_pool[entry->signal_array_offset];
            for (size_t j = 0; j < entry->signal_array_len; j++) {
                size_t signal_index = entry->signal_array_indices[j];
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
    const apg_v2_registry_scalar_refresh_t *items,
    size_t                                  items_len,
    const char                             *node_id,
    const char                             *atom_name,
    apg_v2_runtime_t                       *runtime,
    void                                   *storage,
    uc_error                               *err
) {
    if (!runtime || !node_id || !atom_name || !storage) {
        return set_error(err, UC_E_MISSING, "v2 runtime scalar refresh context is missing");
    }
    runtime->last_error[0] = '\0';
    if (!refresh_scalar_plan_runtime(items, items_len, node_id, atom_name, runtime, storage))
        return set_error(
            err, UC_E_TYPE, runtime->last_error[0] ? runtime->last_error : "v2 runtime scalar refresh failed"
        );
    return UC_OK;
}

static size_t count_param_scalar_refreshes(const apg_v2_registry_scalar_refresh_t *items, size_t items_len) {
    size_t count = 0u;
    for (size_t i = 0; i < items_len; i++) {
        if (items[i].kind == APG_BIND_PARAM)
            count++;
    }
    return count;
}

static uc_status copy_param_scalar_refreshes(
    const apg_v2_registry_scalar_refresh_t *all_items,
    size_t                                  all_items_len,
    apg_v2_registry_scalar_refresh_t      **out_items,
    size_t                                 *out_len,
    uc_error                               *err
) {
    if (!out_items || !out_len)
        return UC_E_TYPE;
    *out_items = NULL;
    *out_len   = 0u;
    if (!all_items || all_items_len == 0u)
        return UC_OK;

    size_t count = count_param_scalar_refreshes(all_items, all_items_len);
    if (count == 0u)
        return UC_OK;

    apg_v2_registry_scalar_refresh_t *param_items = calloc(count, sizeof(*param_items));
    if (!param_items)
        return set_error(err, UC_E_OOM, "v2 runtime scalar refresh allocation failed");

    size_t cursor = 0u;
    for (size_t i = 0; i < all_items_len; i++) {
        if (all_items[i].kind != APG_BIND_PARAM)
            continue;
        param_items[cursor++] = all_items[i];
    }
    *out_items = param_items;
    *out_len   = cursor;
    return UC_OK;
}

static bool refresh_scalar_plan_runtime(
    const apg_v2_registry_scalar_refresh_t *items,
    size_t                                  items_len,
    const char                             *node_id,
    const char                             *atom_name,
    apg_v2_runtime_t                       *runtime,
    void                                   *storage
) {
    if (!runtime || !node_id || !atom_name || !storage)
        return apg_v2_runtime_set_error(runtime, "v2 runtime scalar refresh context is missing"), false;

    for (size_t i = 0; i < items_len; i++) {
        if (items[i].kind != APG_BIND_PARAM && items[i].kind != APG_BIND_LITERAL) {
            char msg[160];
            snprintf(
                msg, sizeof(msg), "node '%s' atom '%s' %s binding key '%s' metadata is missing", node_id, atom_name,
                items[i].config ? "config" : "input", items[i].key ? items[i].key : ""
            );
            apg_v2_runtime_set_error(runtime, msg);
            return false;
        }
        if (items[i].kind == APG_BIND_PARAM) {
            uint32_t param_index = items[i].param_index;
            if (param_index >= runtime->params_len) {
                apg_v2_runtime_set_error(runtime, "v2 runtime scalar refresh param index is out of bounds");
                return false;
            }
        }
        void *addr  = (char *)storage + items[i].storage_offset;
        float value = scalar_refresh_value(&items[i], runtime);
        if (items[i].field_type == FIELD_INT)
            *(int *)addr = (int)value;
        else if (items[i].field_type == FIELD_FLOAT)
            *(float *)addr = value;
        else {
            apg_v2_runtime_set_error(runtime, "v2 runtime scalar refresh field type is unsupported");
            return false;
        }
    }

    return true;
}

static uc_status validate_schedule(const apg_v2_registry_t *registry, uc_error *err) {
    if (registry->schedule_len == 0u)
        return UC_OK;
    if (!registry->schedule)
        return set_error(err, UC_E_MISSING, "v2 registry schedule is missing");
    for (size_t i = 0; i < registry->schedule_len; i++) {
        if (registry->schedule[i] >= registry->nodes_len)
            return set_error(err, UC_E_RANGE, "v2 runtime schedule index is out of range");
    }
    return UC_OK;
}

static uc_status
apply_mix_matrix_config(const apg_v2_registry_node_layout_t *layout, apg_v2_runtime_node_t *node, uc_error *err) {
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
    const apg_v2_registry_node_layout_t *layout, apg_v2_runtime_t *runtime, apg_v2_runtime_node_t *node, uc_error *err
) {
    if (!layout || layout->state_buffers_len == 0u)
        return UC_OK;

    if (!runtime || !runtime->state_buffer_ptrs || !runtime->state_buffer_sample_counts || !runtime->state_buffer_pool)
        return set_error(err, UC_E_OOM, "v2 runtime state buffer table allocation failed");
    if (layout->state_buffer_table_offset + layout->state_buffers_len > runtime->state_buffer_count)
        return set_error(err, UC_E_RANGE, "v2 runtime state buffer table index out of range");

    node->state_buffers        = runtime->state_buffer_ptrs + layout->state_buffer_table_offset;
    node->state_buffer_samples = runtime->state_buffer_sample_counts + layout->state_buffer_table_offset;
    node->state_buffers_len    = layout->state_buffers_len;

    if (node->state_buffers_len > 0u && (!node->state_buffers || !node->state_buffer_samples)) {
        char msg[192];
        snprintf(
            msg, sizeof(msg), "node '%s' atom '%s' state buffer allocation failed",
            layout->node_id ? layout->node_id : "", layout->atom_name ? layout->atom_name : ""
        );
        return set_error(err, UC_E_OOM, msg);
    }

    size_t buffer_index = 0;
    for (int i = 0; i < layout->n_state_fields; i++) {
        const atom_field_desc_t *field = &layout->state_fields[i];
        if (field->type != FIELD_BUFFER)
            continue;
        size_t buffer_samples = layout->state_buffer_samples_by_index
                                    ? layout->state_buffer_samples_by_index[buffer_index]
                                    : field->buffer_samples;
        if (buffer_samples == 0u) {
            char msg[192];
            snprintf(
                msg, sizeof(msg), "node '%s' atom '%s' state binding key '%s' is missing buffer capacity",
                layout->node_id ? layout->node_id : "", layout->atom_name ? layout->atom_name : "",
                field->name ? field->name : ""
            );
            return set_error(err, UC_E_MISSING, msg);
        }
        size_t offset = layout->state_buffer_sample_offsets_by_index
                            ? layout->state_buffer_sample_offsets_by_index[buffer_index]
                            : 0u;
        if (!runtime || !runtime->state_buffer_pool || buffer_samples > SIZE_MAX - offset ||
            offset + buffer_samples > runtime->state_buffer_samples)
            return set_error(err, UC_E_RANGE, "v2 runtime state buffer layout exceeds pool");
        float *buffer                            = &runtime->state_buffer_pool[offset];
        node->state_buffers[buffer_index]        = buffer;
        node->state_buffer_samples[buffer_index] = buffer_samples;
        buffer_index++;
        float **field_ptr = (float **)((char *)node->state_storage + field->offset);
        *field_ptr        = buffer;
        for (int length_index = 0; length_index < layout->n_state_fields; length_index++) {
            const atom_field_desc_t *length_field = &layout->state_fields[length_index];
            if (length_field->type == FIELD_INT && length_field->name &&
                strcmp(length_field->name, "buffer_len") == 0) {
                if (buffer_samples > UINT32_MAX)
                    return set_error(err, UC_E_RANGE, "v2 runtime state buffer length exceeds uint32 range");
                uint32_t *length_ptr = (uint32_t *)((char *)node->state_storage + length_field->offset);
                *length_ptr          = (uint32_t)buffer_samples;
                break;
            }
        }
    }
    return UC_OK;
}

static uc_status init_node_calls(const apg_v2_registry_t *registry, apg_v2_runtime_t *out, uc_error *err) {
    uc_status status = UC_OK;
    out->nodes_len   = registry->nodes_len;
    if (out->nodes_len == 0u)
        return UC_OK;
    if (!registry->node_layouts)
        return set_error(err, UC_E_MISSING, "v2 registry node layouts are missing");

    out->nodes = calloc(out->nodes_len, sizeof(*out->nodes));
    if (!out->nodes)
        return set_error(err, UC_E_OOM, "v2 runtime node allocation failed");
    out->atom_storage_bytes = registry->atom_storage_bytes;
    if (out->atom_storage_bytes > 0u) {
        out->atom_storage_pool = calloc(1u, out->atom_storage_bytes);
        if (!out->atom_storage_pool)
            return set_error(err, UC_E_OOM, "v2 runtime atom storage pool allocation failed");
    }
    out->state_buffer_samples = registry->state_buffer_samples;
    if (out->state_buffer_samples > 0u) {
        out->state_buffer_pool = calloc(out->state_buffer_samples, sizeof(*out->state_buffer_pool));
        if (!out->state_buffer_pool)
            return set_error(err, UC_E_OOM, "v2 runtime state buffer pool allocation failed");
    }
    out->state_buffer_count = registry->state_buffers_len;
    if (out->state_buffer_count > 0u) {
        if (out->state_buffer_count > SIZE_MAX / sizeof(*out->state_buffer_ptrs))
            return set_error(err, UC_E_RANGE, "v2 runtime state buffer table is too large");
        out->state_buffer_ptrs = calloc(out->state_buffer_count, sizeof(*out->state_buffer_ptrs));
        if (!out->state_buffer_ptrs)
            return set_error(err, UC_E_OOM, "v2 runtime state buffer pointer table allocation failed");
        if (out->state_buffer_count > SIZE_MAX / sizeof(*out->state_buffer_sample_counts))
            return set_error(err, UC_E_RANGE, "v2 runtime state buffer sample-table is too large");
        out->state_buffer_sample_counts = calloc(out->state_buffer_count, sizeof(*out->state_buffer_sample_counts));
        if (!out->state_buffer_sample_counts)
            return set_error(err, UC_E_OOM, "v2 runtime state buffer sample table allocation failed");
    }
    if (registry->signal_array_pointer_slots > 0u) {
        out->signal_array_pool_len = registry->signal_array_pointer_slots;
        out->signal_array_pool     = calloc(out->signal_array_pool_len, sizeof(*out->signal_array_pool));
        if (!out->signal_array_pool)
            return set_error(err, UC_E_OOM, "v2 runtime signal array pool allocation failed");
    }

    for (size_t i = 0; i < out->nodes_len; i++) {
        apg_v2_runtime_node_t               *node   = &out->nodes[i];
        const apg_v2_registry_node_layout_t *layout = &registry->node_layouts[i];
        if (!layout->thunk || !layout->atom_name)
            return set_error(err, UC_E_MISSING, "v2 runtime node is missing atom metadata");

        if (layout->out_offset + layout->out_size > out->atom_storage_bytes ||
            layout->in_offset + layout->in_size > out->atom_storage_bytes ||
            layout->config_offset + layout->config_size > out->atom_storage_bytes ||
            layout->state_offset + layout->state_size > out->atom_storage_bytes)
            return set_error(err, UC_E_RANGE, "v2 registry atom storage layout exceeds pool");
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
        node->node_id                = layout->node_id;
        node->atom_name              = layout->atom_name;
        node->thunk                  = layout->thunk;
        node->state_fields           = layout->state_fields;
        node->n_state_fields         = layout->n_state_fields;
        node->state_size             = layout->state_size;

        node->call.out           = node->out_storage;
        node->call.in            = node->in_storage;
        node->call.config        = node->config_storage;
        node->call.state         = node->state_storage;
        node->call.info          = &out->process_info;
        node->call.spectral_info = layout->has_spectral_info ? &layout->spectral_info : NULL;

        status = init_state_buffers(layout, out, node, err);
        if (status != UC_OK)
            return status;
        node->signal_bindings     = layout->signal_bindings;
        node->signal_bindings_len = layout->signal_bindings_len;
        status                    = apply_signal_bindings(layout, out, node, err);
        if (status != UC_OK)
            return status;
        status = refresh_scalar_plan(
            node->config_refreshes, node->config_refreshes_len, node->node_id, node->atom_name, out,
            node->config_storage, err
        );
        if (status != UC_OK)
            return status;
        status = copy_param_scalar_refreshes(
            node->config_refreshes, node->config_refreshes_len, &node->config_refreshes_runtime,
            &node->config_refreshes_runtime_len, err
        );
        if (status != UC_OK)
            return status;
        status = refresh_scalar_plan(
            node->input_refreshes, node->input_refreshes_len, node->node_id, node->atom_name, out, node->in_storage, err
        );
        if (status != UC_OK)
            return status;
        status = copy_param_scalar_refreshes(
            node->input_refreshes, node->input_refreshes_len, &node->input_refreshes_runtime,
            &node->input_refreshes_runtime_len, err
        );
        if (status != UC_OK)
            return status;
        status = apply_mix_matrix_config(layout, node, err);
        if (status != UC_OK)
            return status;
    }

    if ((status = validate_schedule(registry, err)) != UC_OK)
        return status;

    return UC_OK;
}

uc_status apg_v2_runtime_init_from_registry(const apg_v2_registry_t *registry, apg_v2_runtime_t *out, uc_error *err) {
    if (!registry || !out || !err)
        return UC_E_TYPE;
    memset(out, 0, sizeof(*out));
    err->status = UC_OK;
    if (registry->frame_capacity == 0u)
        return set_error(err, UC_E_RANGE, "v2 runtime frame capacity must be greater than zero");

    out->frame_capacity             = registry->frame_capacity;
    out->process_info.sample_rate   = registry->sample_rate;
    out->process_info.frames        = registry->frame_capacity;
    out->process_info.output_frames = registry->frame_capacity;
    out->process_info.channels      = 1u;

    uc_status status = init_signal_buffers(registry, out, err);
    if (status != UC_OK)
        goto fail;
    out->signal_names           = registry->signal_names;
    out->input_meters_len       = registry->input_meters_len;
    out->output_meters_len      = registry->output_meters_len;
    out->input_audio_ports      = registry->input_audio_ports;
    out->input_audio_ports_len  = registry->input_audio_ports_len;
    out->output_audio_ports     = registry->output_audio_ports;
    out->output_audio_ports_len = registry->output_audio_ports_len;
    status                      = init_params(registry, out, err);
    if (status != UC_OK)
        goto fail;
    out->param_names         = registry->param_names;
    out->control_targets     = registry->control_targets;
    out->control_targets_len = registry->control_targets_len;
    status                   = init_node_calls(registry, out, err);
    if (status != UC_OK)
        goto fail;
    out->schedule     = registry->schedule;
    out->schedule_len = registry->schedule_len;
    status            = init_bypass_state_from_registry(registry, out, err);
    if (status != UC_OK)
        goto fail;
    out->bypass_index_by_node            = registry->bypass_index_by_node;
    out->project_mute_output_indices     = registry->project_mute_output_indices;
    out->project_mute_output_indices_len = registry->project_mute_output_indices_len;
    if (out->nodes_len != 0u && registry->bypassed_instances_len > 0u && !out->bypass_index_by_node) {
        status = set_error(err, UC_E_MISSING, "v2 runtime bypass index map is missing from registry");
        goto fail;
    }
    return UC_OK;

fail:
    apg_v2_runtime_destroy(out);
    return status;
}

uc_status
apg_v2_runtime_create_from_registry(const apg_v2_registry_t *registry, apg_v2_runtime_t **out, uc_error *err) {
    if (!out || !err)
        return UC_E_TYPE;
    *out = NULL;
    if (!registry)
        return set_error(err, UC_E_MISSING, "v2 registry is missing");

    apg_v2_runtime_t *runtime = calloc(1, sizeof(*runtime));
    if (!runtime)
        return set_error(err, UC_E_OOM, "v2 runtime allocation failed");

    uc_status status = apg_v2_runtime_init_from_registry(registry, runtime, err);
    if (status != UC_OK) {
        free(runtime);
        return status;
    }
    *out = runtime;
    return UC_OK;
}

const float *apg_v2_runtime_signal_buffer_at(const apg_v2_runtime_t *runtime, size_t signal_index) {
    if (!runtime || signal_index >= runtime->signals_len || !runtime->signals)
        return NULL;
    return runtime->signals[signal_index];
}

float *apg_v2_runtime_signal_buffer_at_mut(apg_v2_runtime_t *runtime, size_t signal_index) {
    if (!runtime || signal_index >= runtime->signals_len || !runtime->signals)
        return NULL;
    return runtime->signals[signal_index];
}

bool apg_v2_runtime_input_port_channel_signal_index(
    const apg_v2_runtime_t *runtime,
    size_t                  port_index,
    size_t                  channel_index,
    size_t                 *out_signal_index,
    size_t                 *out_meter_index
) {
    if (!runtime || port_index >= runtime->input_audio_ports_len)
        return false;
    const apg_v2_registry_audio_port_t *port = &runtime->input_audio_ports[port_index];
    if (!port->signal_indices || port->channel_count == 0u || channel_index >= port->channel_count)
        return false;
    if (port->signal_indices[channel_index] >= runtime->signals_len)
        return false;
    if (out_signal_index)
        *out_signal_index = port->signal_indices[channel_index];
    if (out_meter_index)
        *out_meter_index = port->meter_index + channel_index;
    return true;
}

bool apg_v2_runtime_output_port_channel_signal_index(
    const apg_v2_runtime_t *runtime,
    size_t                  port_index,
    size_t                  channel_index,
    size_t                 *out_signal_index,
    size_t                 *out_meter_index
) {
    if (!runtime || port_index >= runtime->output_audio_ports_len)
        return false;
    const apg_v2_registry_audio_port_t *port = &runtime->output_audio_ports[port_index];
    if (!port->signal_indices || port->channel_count == 0u || channel_index >= port->channel_count)
        return false;
    if (port->signal_indices[channel_index] >= runtime->signals_len)
        return false;
    if (out_signal_index)
        *out_signal_index = port->signal_indices[channel_index];
    if (out_meter_index)
        *out_meter_index = port->meter_index + channel_index;
    return true;
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
        apg_v2_runtime_set_error(runtime, "v2 runtime instance bypass signal lookup failed");
        return false;
    }
    memcpy(runtime->signals[entry->output_index], runtime->signals[entry->input_index], frames * sizeof(float));
    return true;
}

void apg_v2_runtime_apply_project_mute(apg_v2_runtime_t *runtime, uint32_t frames) {
    if (!runtime || !runtime->project_muted || runtime->project_mute_output_indices_len == 0u)
        return;
    for (size_t i = 0; i < runtime->project_mute_output_indices_len; i++) {
        size_t index = runtime->project_mute_output_indices[i];
        if (index < runtime->signals_len && runtime->signals[index])
            memset(runtime->signals[index], 0, frames * sizeof(float));
    }
}

bool apg_v2_runtime_run_node(apg_v2_runtime_t *runtime, size_t node_index, uint32_t frames) {
    if (!runtime)
        return false;

    apg_v2_runtime_node_t *node = &runtime->nodes[node_index];
    if (!node->thunk) {
        apg_v2_runtime_set_error(runtime, "v2 runtime node metadata is missing");
        return false;
    }

    size_t bypass_index = runtime_node_bypass_index(runtime, node_index);
    if (bypass_index != INVALID_BYPASS_INDEX && runtime->bypassed_instances[bypass_index].enabled) {
        if (!apply_instance_bypass(runtime, &runtime->bypassed_instances[bypass_index], frames))
            return false;
        return true;
    }

    if (!refresh_scalar_plan_runtime(
            node->config_refreshes_runtime, node->config_refreshes_runtime_len, node->node_id, node->atom_name, runtime,
            node->config_storage
        ))
        return false;
    if (!refresh_scalar_plan_runtime(
            node->input_refreshes_runtime, node->input_refreshes_runtime_len, node->node_id, node->atom_name, runtime,
            node->in_storage
        ))
        return false;

    node->thunk(&node->call);
    return true;
}

bool apg_v2_runtime_set_param_index(apg_v2_runtime_t *runtime, size_t index, float value) {
    if (!runtime || index >= runtime->params_len)
        return false;
    uint32_t smoothing_frames =
        runtime->has_processed && runtime->param_smoothing_frames ? runtime->param_smoothing_frames[index] : 0u;
    if (!runtime->param_targets || !runtime->param_smoothing_remaining_frames || smoothing_frames == 0u) {
        runtime->params[index] = value;
        if (runtime->param_targets)
            runtime->param_targets[index] = value;
        if (runtime->param_smoothing_remaining_frames)
            runtime->param_smoothing_remaining_frames[index] = 0u;
        return true;
    }
    runtime->param_targets[index]                    = value;
    runtime->param_smoothing_remaining_frames[index] = smoothing_frames;
    return true;
}

bool apg_v2_runtime_set_control_port_index(apg_v2_runtime_t *runtime, size_t control_target_index, float value) {
    if (!runtime || control_target_index >= runtime->control_targets_len)
        return false;
    return apg_v2_runtime_set_param_index(runtime, runtime->control_targets[control_target_index].param_index, value);
}

bool apg_v2_runtime_set_instance_bypass_index(apg_v2_runtime_t *runtime, size_t bypass_index, bool enabled) {
    if (!runtime || bypass_index >= runtime->bypassed_instances_len)
        return false;
    runtime->bypassed_instances[bypass_index].enabled = enabled;
    return true;
}

bool apg_v2_runtime_set_project_mute(apg_v2_runtime_t *runtime, bool muted) {
    if (!runtime)
        return false;
    runtime->project_muted = muted;
    return true;
}

static bool reset_node_state(apg_v2_runtime_node_t *node) {
    if (!node || !node->state_storage)
        return true;
    if (node->n_state_fields < 0 || (node->n_state_fields > 0 && !node->state_fields))
        return false;

    memset(node->state_storage, 0, atom_storage_size(node->state_size));

    size_t buffer_index = 0;
    for (int field_index = 0; field_index < node->n_state_fields; field_index++) {
        const atom_field_desc_t *field = &node->state_fields[field_index];
        if (field->type != FIELD_BUFFER)
            continue;
        if (buffer_index >= node->state_buffers_len || !node->state_buffers[buffer_index])
            return false;
        memset(node->state_buffers[buffer_index], 0, node->state_buffer_samples[buffer_index] * sizeof(float));
        float **field_ptr = (float **)((char *)node->state_storage + field->offset);
        *field_ptr        = node->state_buffers[buffer_index];
        for (int length_index = 0; length_index < node->n_state_fields; length_index++) {
            const atom_field_desc_t *length_field = &node->state_fields[length_index];
            if (length_field->type == FIELD_INT && length_field->name &&
                strcmp(length_field->name, "buffer_len") == 0) {
                if (node->state_buffer_samples[buffer_index] > UINT32_MAX)
                    return false;
                uint32_t *length_ptr = (uint32_t *)((char *)node->state_storage + length_field->offset);
                *length_ptr          = (uint32_t)node->state_buffer_samples[buffer_index];
                break;
            }
        }
        buffer_index++;
    }
    return true;
}

static bool reset_dispatch(apg_runtime_reset_context_t *ctx) {
    if (!ctx || !ctx->runtime)
        return false;

    while (ctx->state != APG_RUNTIME_RESET_DONE && ctx->state != APG_RUNTIME_RESET_FAIL) {
        switch (ctx->state) {
        case APG_RUNTIME_RESET_BEGIN:
            ctx->runtime->last_error[0] = '\0';
            ctx->state                  = APG_RUNTIME_RESET_CLEAR_SIGNALS;
            break;
        case APG_RUNTIME_RESET_CLEAR_SIGNALS:
            if (ctx->runtime->signal_pool && ctx->runtime->signals_len > 0u) {
                memset(
                    ctx->runtime->signal_pool, 0,
                    ctx->runtime->signals_len * (size_t)ctx->runtime->frame_capacity *
                        sizeof(*ctx->runtime->signal_pool)
                );
            }
            ctx->state = APG_RUNTIME_RESET_RESTORE_PARAMS;
            break;
        case APG_RUNTIME_RESET_RESTORE_PARAMS:
            for (size_t i = 0; i < ctx->runtime->params_len; i++) {
                ctx->runtime->params[i] = ctx->runtime->param_defaults ? ctx->runtime->param_defaults[i] : 0.0f;
                if (ctx->runtime->param_targets)
                    ctx->runtime->param_targets[i] = ctx->runtime->params[i];
                if (ctx->runtime->param_smoothing_remaining_frames)
                    ctx->runtime->param_smoothing_remaining_frames[i] = 0u;
            }
            ctx->runtime->has_processed = false;
            ctx->node_cursor            = 0u;
            ctx->state                  = APG_RUNTIME_RESET_RESET_NODE;
            break;
        case APG_RUNTIME_RESET_RESET_NODE:
            if (ctx->node_cursor >= ctx->runtime->nodes_len) {
                ctx->state = APG_RUNTIME_RESET_RESET_PROCESS_INFO;
                break;
            }
            if (!reset_node_state(&ctx->runtime->nodes[ctx->node_cursor++])) {
                ctx->state = APG_RUNTIME_RESET_FAIL;
                return false;
            }
            break;
        case APG_RUNTIME_RESET_RESET_PROCESS_INFO:
            ctx->runtime->process_info.frames        = ctx->runtime->frame_capacity;
            ctx->runtime->process_info.output_frames = ctx->runtime->frame_capacity;
            ctx->runtime->process_info.channels      = 1u;
            ctx->state                               = APG_RUNTIME_RESET_DONE;
            break;
        case APG_RUNTIME_RESET_DONE:
        case APG_RUNTIME_RESET_FAIL:
            break;
        }
    }

    return ctx->state == APG_RUNTIME_RESET_DONE;
}

bool apg_v2_runtime_reset(apg_v2_runtime_t *runtime) {
    apg_runtime_reset_context_t ctx = {
        .runtime = runtime,
        .state   = APG_RUNTIME_RESET_BEGIN,
    };
    return reset_dispatch(&ctx);
}

bool apg_v2_runtime_process(apg_v2_runtime_t *runtime, uint32_t frames) {
    return apg_v2_runtime_dispatch_process(runtime, frames);
}

bool apg_v2_runtime_process_interleaved_port_indices(
    apg_v2_runtime_t *runtime,
    size_t            input_port_index,
    const float      *input,
    size_t            output_port_index,
    float            *output,
    uint32_t          frames
) {
    const apg_v2_registry_audio_port_t *input_port  = runtime && input_port_index < runtime->input_audio_ports_len
                                                          ? &runtime->input_audio_ports[input_port_index]
                                                          : NULL;
    const apg_v2_registry_audio_port_t *output_port = runtime && output_port_index < runtime->output_audio_ports_len
                                                          ? &runtime->output_audio_ports[output_port_index]
                                                          : NULL;
    return apg_v2_runtime_dispatch_process_interleaved_ports(runtime, input_port, input, output_port, output, frames);
}

static bool runtime_process_mono_audio_ports(
    apg_v2_runtime_t                   *runtime,
    const apg_v2_registry_audio_port_t *input_port,
    const float                        *input,
    const apg_v2_registry_audio_port_t *output_port,
    float                              *output,
    uint32_t                            frames
) {
    return apg_v2_runtime_dispatch_process_mono_audio_ports(runtime, input_port, input, output_port, output, frames);
}

bool apg_v2_runtime_process_mono_port_indices(
    apg_v2_runtime_t *runtime,
    size_t            input_port_index,
    const float      *input,
    size_t            output_port_index,
    float            *output,
    uint32_t          frames
) {
    const apg_v2_registry_audio_port_t *input_port  = runtime && input_port_index < runtime->input_audio_ports_len
                                                          ? &runtime->input_audio_ports[input_port_index]
                                                          : NULL;
    const apg_v2_registry_audio_port_t *output_port = runtime && output_port_index < runtime->output_audio_ports_len
                                                          ? &runtime->output_audio_ports[output_port_index]
                                                          : NULL;
    return runtime_process_mono_audio_ports(runtime, input_port, input, output_port, output, frames);
}

bool apg_v2_runtime_process_mono(apg_v2_runtime_t *runtime, const float *input, float *output, uint32_t frames) {
    if (!runtime)
        return false;

    return runtime_process_mono_audio_ports(
        runtime, runtime->input_audio_ports_len > 0u ? &runtime->input_audio_ports[0] : NULL, input,
        runtime->output_audio_ports_len > 0u ? &runtime->output_audio_ports[0] : NULL, output, frames
    );
}

void apg_v2_runtime_destroy(apg_v2_runtime_t *runtime) {
    if (!runtime)
        return;

    for (size_t i = 0; i < runtime->nodes_len; i++) {
        free(runtime->nodes[i].config_refreshes_runtime);
        free(runtime->nodes[i].input_refreshes_runtime);
    }
    free(runtime->signal_array_pool);
    free(runtime->nodes);
    free(runtime->atom_storage_pool);
    free(runtime->state_buffer_pool);
    free(runtime->state_buffer_ptrs);
    free(runtime->state_buffer_sample_counts);
    free(runtime->bypassed_instances);
    free(runtime->params);
    free(runtime->param_defaults);
    free(runtime->param_targets);
    free(runtime->param_smoothing_remaining_frames);
    free(runtime->signals);
    free(runtime->signal_pool);
    memset(runtime, 0, sizeof(*runtime));
}

void apg_v2_runtime_destroy_owned(apg_v2_runtime_t **runtime) {
    if (!runtime || !*runtime)
        return;
    apg_v2_runtime_destroy(*runtime);
    free(*runtime);
    *runtime = NULL;
}
