#include <apgcore/runtime_v2.h>

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static uc_status set_error(uc_error *err, uc_status status, const char *msg) {
    uc_loc loc = {0, 0};
    uc_error_set(err, status, loc, "%s", msg);
    return status;
}

#define APG_V2_STATE_BUFFER_SAMPLES 192000u

static size_t atom_storage_size(size_t size) { return size > 0u ? size : 1u; }

static int signal_index_by_name(const apg_unit_v2_t *unit, const char *name) {
    if (!unit || !name)
        return -1;
    for (size_t i = 0; i < unit->signals_len; i++) {
        if (unit->signals[i] && strcmp(unit->signals[i], name) == 0)
            return (int)i;
    }
    return -1;
}

static int first_audio_port_signal_index(const apg_unit_v2_t *unit, const apg_unit_v2_port_t *ports, size_t ports_len) {
    for (size_t i = 0; i < ports_len; i++) {
        if (ports[i].type && strcmp(ports[i].type, "audio") == 0)
            return signal_index_by_name(unit, ports[i].name);
    }
    return -1;
}

static float parse_param_default(const apg_unit_v2_param_t *param) {
    if (!param || !param->default_value)
        return 0.0f;
    if (param->type && strcmp(param->type, "bool") == 0)
        return strcmp(param->default_value, "true") == 0 ? 1.0f : 0.0f;
    return strtof(param->default_value, NULL);
}

static uc_status
init_signal_buffers(const apg_v2_compiled_unit_t *plan, uint32_t frame_capacity, apg_v2_runtime_t *out, uc_error *err) {
    out->signals_len = plan->unit->signals_len;
    if (out->signals_len == 0u)
        return UC_OK;
    if (frame_capacity > 0u && out->signals_len > SIZE_MAX / (size_t)frame_capacity / sizeof(float))
        return set_error(err, UC_E_RANGE, "v2 runtime signal pool is too large");

    out->signals     = calloc(out->signals_len, sizeof(*out->signals));
    out->signal_pool = calloc(out->signals_len * (size_t)frame_capacity, sizeof(*out->signal_pool));
    if (!out->signals || !out->signal_pool)
        return set_error(err, UC_E_OOM, "v2 runtime signal allocation failed");

    for (size_t i = 0; i < out->signals_len; i++)
        out->signals[i] = &out->signal_pool[i * (size_t)frame_capacity];
    return UC_OK;
}

static uc_status init_params(const apg_v2_compiled_unit_t *plan, apg_v2_runtime_t *out, uc_error *err) {
    out->params_len = plan->unit->params_len;
    if (out->params_len == 0u)
        return UC_OK;

    out->params = calloc(out->params_len, sizeof(*out->params));
    if (!out->params)
        return set_error(err, UC_E_OOM, "v2 runtime param allocation failed");

    for (size_t i = 0; i < out->params_len; i++)
        out->params[i] = parse_param_default(&plan->unit->params[i]);
    return UC_OK;
}

static uc_status bind_signal_fields(
    const apg_v2_compiled_binding_t *bindings, size_t bindings_len, apg_v2_runtime_t *out, void *storage, uc_error *err
) {
    float **fields = (float **)storage;
    for (size_t i = 0; i < bindings_len; i++) {
        if (bindings[i].kind != APG_BIND_SIGNAL || bindings[i].index >= out->signals_len)
            return set_error(err, UC_E_MISSING, "v2 runtime signal binding is invalid");
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
    if (binding->kind == APG_BIND_PARAM)
        return binding->index < runtime->params_len ? runtime->params[binding->index] : 0.0f;
    if (binding->kind == APG_BIND_LITERAL)
        return binding->literal ? strtof(binding->literal, NULL) : 0.0f;
    return 0.0f;
}

static uc_status refresh_node_config(const apg_v2_compiled_node_t *compiled, apg_v2_runtime_t *runtime, uc_error *err) {
    apg_v2_runtime_node_t *node = &runtime->nodes[compiled - runtime->plan->nodes];
    for (size_t i = 0; i < compiled->config_len; i++) {
        const atom_field_desc_t *field = find_config_field(compiled->atom, compiled->config[i].key);
        if (!field)
            return set_error(err, UC_E_MISSING, "v2 runtime config field metadata is missing");

        void *addr  = (char *)node->config_storage + field->offset;
        float value = compiled_config_value(&compiled->config[i], runtime);
        if (field->type == FIELD_INT)
            *(int *)addr = (int)value;
        else if (field->type == FIELD_FLOAT)
            *(float *)addr = value;
        else
            return set_error(err, UC_E_TYPE, "v2 runtime config field type is not scalar");
    }
    return UC_OK;
}

static uc_status init_state_buffers(const atom_registry_entry_t *atom, apg_v2_runtime_node_t *node, uc_error *err) {
    size_t buffer_count = 0;
    for (int i = 0; i < atom->n_state_fields; i++) {
        if (atom->state_fields[i].type == FIELD_BUFFER)
            buffer_count++;
    }
    if (buffer_count == 0u)
        return UC_OK;

    node->state_buffers = calloc(buffer_count, sizeof(*node->state_buffers));
    if (!node->state_buffers)
        return set_error(err, UC_E_OOM, "v2 runtime state buffer allocation failed");
    node->state_buffers_len = buffer_count;

    size_t buffer_index = 0;
    for (int i = 0; i < atom->n_state_fields; i++) {
        const atom_field_desc_t *field = &atom->state_fields[i];
        if (field->type != FIELD_BUFFER)
            continue;
        float *buffer = calloc(APG_V2_STATE_BUFFER_SAMPLES, sizeof(*buffer));
        if (!buffer)
            return set_error(err, UC_E_OOM, "v2 runtime state buffer allocation failed");
        node->state_buffers[buffer_index++] = buffer;
        float **field_ptr                   = (float **)((char *)node->state_storage + field->offset);
        *field_ptr                          = buffer;
    }
    return UC_OK;
}

static uc_status init_node_calls(const apg_v2_compiled_unit_t *plan, apg_v2_runtime_t *out, uc_error *err) {
    out->nodes_len = plan->nodes_len;
    if (out->nodes_len == 0u)
        return UC_OK;

    out->nodes = calloc(out->nodes_len, sizeof(*out->nodes));
    if (!out->nodes)
        return set_error(err, UC_E_OOM, "v2 runtime node allocation failed");

    for (size_t i = 0; i < out->nodes_len; i++) {
        const atom_registry_entry_t *atom = plan->nodes[i].atom;
        apg_v2_runtime_node_t       *node = &out->nodes[i];
        if (!atom)
            return set_error(err, UC_E_MISSING, "v2 runtime node is missing atom metadata");

        node->out_storage    = calloc(1u, atom_storage_size(atom->out_size));
        node->in_storage     = calloc(1u, atom_storage_size(atom->in_size));
        node->config_storage = calloc(1u, atom_storage_size(atom->config_size));
        node->state_storage  = calloc(1u, atom_storage_size(atom->state_size));
        if (!node->out_storage || !node->in_storage || !node->config_storage || !node->state_storage)
            return set_error(err, UC_E_OOM, "v2 runtime atom call allocation failed");

        node->call.out    = node->out_storage;
        node->call.in     = node->in_storage;
        node->call.config = node->config_storage;
        node->call.state  = node->state_storage;
        node->call.info   = &out->process_info;

        uc_status status = init_state_buffers(atom, node, err);
        if (status != UC_OK)
            return status;
        status = bind_signal_fields(plan->nodes[i].out, plan->nodes[i].out_len, out, node->out_storage, err);
        if (status != UC_OK)
            return status;
        status = bind_signal_fields(plan->nodes[i].in, plan->nodes[i].in_len, out, node->in_storage, err);
        if (status != UC_OK)
            return status;
        status = refresh_node_config(&plan->nodes[i], out, err);
        if (status != UC_OK)
            return status;
    }
    return UC_OK;
}

uc_status apg_v2_runtime_init(
    const apg_v2_compiled_unit_t *plan, uint32_t frame_capacity, float sample_rate, apg_v2_runtime_t *out, uc_error *err
) {
    if (!plan || !plan->unit || !out || !err)
        return UC_E_TYPE;
    memset(out, 0, sizeof(*out));
    err->status = UC_OK;
    if (frame_capacity == 0u)
        return set_error(err, UC_E_RANGE, "v2 runtime frame capacity must be greater than zero");

    out->plan                       = plan;
    out->frame_capacity             = frame_capacity;
    out->process_info.sample_rate   = sample_rate > 0.0f ? sample_rate : 48000.0f;
    out->process_info.frames        = frame_capacity;
    out->process_info.output_frames = frame_capacity;
    out->process_info.channels      = 1u;

    uc_status status = init_signal_buffers(plan, frame_capacity, out, err);
    if (status != UC_OK)
        goto fail;
    status = init_params(plan, out, err);
    if (status != UC_OK)
        goto fail;
    status = init_node_calls(plan, out, err);
    if (status != UC_OK)
        goto fail;
    return UC_OK;

fail:
    apg_v2_runtime_destroy(out);
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

bool apg_v2_runtime_set_param(apg_v2_runtime_t *runtime, const char *name, float value) {
    if (!runtime || !runtime->plan || !runtime->plan->unit || !name)
        return false;
    const apg_unit_v2_t *unit = runtime->plan->unit;
    for (size_t i = 0; i < unit->params_len && i < runtime->params_len; i++) {
        if (unit->params[i].name && strcmp(unit->params[i].name, name) == 0) {
            runtime->params[i] = value;
            return true;
        }
    }
    return false;
}

bool apg_v2_runtime_process(apg_v2_runtime_t *runtime, uint32_t frames) {
    if (!runtime || !runtime->plan || !runtime->plan->unit || frames == 0u)
        return false;
    if (frames > runtime->frame_capacity)
        return false;

    runtime->process_info.frames        = frames;
    runtime->process_info.output_frames = frames;

    uc_error err = {0};
    for (size_t i = 0; i < runtime->plan->schedule_len; i++) {
        uint32_t scheduled_index = runtime->plan->schedule[i];
        if (scheduled_index >= runtime->nodes_len)
            return false;
        const apg_v2_compiled_node_t *compiled = &runtime->plan->nodes[scheduled_index];
        if (refresh_node_config(compiled, runtime, &err) != UC_OK)
            return false;
        compiled->atom->thunk(&runtime->nodes[scheduled_index].call);
    }
    return true;
}

bool apg_v2_runtime_process_mono(apg_v2_runtime_t *runtime, const float *input, float *output, uint32_t frames) {
    if (!runtime || !runtime->plan || !runtime->plan->unit || !input || !output)
        return false;

    const apg_unit_v2_t *unit         = runtime->plan->unit;
    int                  input_index  = first_audio_port_signal_index(unit, unit->input_ports, unit->input_ports_len);
    int                  output_index = first_audio_port_signal_index(unit, unit->output_ports, unit->output_ports_len);
    if (input_index < 0 || output_index < 0 || (size_t)input_index >= runtime->signals_len ||
        (size_t)output_index >= runtime->signals_len)
        return false;

    if (frames > runtime->frame_capacity || frames == 0u)
        return false;
    memcpy(runtime->signals[input_index], input, frames * sizeof(float));
    if (!apg_v2_runtime_process(runtime, frames))
        return false;
    memcpy(output, runtime->signals[output_index], frames * sizeof(float));
    return true;
}

void apg_v2_runtime_destroy(apg_v2_runtime_t *runtime) {
    if (!runtime)
        return;

    for (size_t i = 0; i < runtime->nodes_len; i++) {
        for (size_t j = 0; j < runtime->nodes[i].state_buffers_len; j++)
            free(runtime->nodes[i].state_buffers[j]);
        free(runtime->nodes[i].state_buffers);
        free(runtime->nodes[i].out_storage);
        free(runtime->nodes[i].in_storage);
        free(runtime->nodes[i].config_storage);
        free(runtime->nodes[i].state_storage);
    }
    free(runtime->nodes);
    free(runtime->params);
    free(runtime->signals);
    free(runtime->signal_pool);
    memset(runtime, 0, sizeof(*runtime));
}
