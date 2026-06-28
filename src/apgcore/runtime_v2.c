#include <apgcore/runtime_v2.h>

#include <stdlib.h>
#include <string.h>

static uc_status set_error(uc_error *err, uc_status status, const char *msg) {
    uc_loc loc = {0, 0};
    uc_error_set(err, status, loc, "%s", msg);
    return status;
}

static size_t atom_storage_size(size_t size) { return size > 0u ? size : 1u; }

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

void apg_v2_runtime_destroy(apg_v2_runtime_t *runtime) {
    if (!runtime)
        return;

    for (size_t i = 0; i < runtime->nodes_len; i++) {
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
