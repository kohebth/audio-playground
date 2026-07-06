#ifndef AUDIO_PLAYGROUND_TEST_RUNTIME_V2_HARNESS_H
#define AUDIO_PLAYGROUND_TEST_RUNTIME_V2_HARNESS_H

#include <apgcore/registry/registry_builder_v2.h>
#include <apgcore/runtime/runtime_v2.h>
#include <apgcore/runtime/runtime_v2_internal.h>

#include <string.h>

static bool test_runtime_signal_index_by_name(const apg_v2_runtime_t *runtime, const char *name, size_t *out_index) {
    if (!runtime || !name || !out_index)
        return false;
    for (size_t i = 0; i < runtime->signals_len; i++) {
        if (runtime->signal_names[i] && strcmp(runtime->signal_names[i], name) == 0) {
            *out_index = i;
            return true;
        }
    }
    return false;
}

static bool test_runtime_param_index_by_name(const apg_v2_runtime_t *runtime, const char *name, size_t *out_index) {
    if (!runtime || !name || !out_index)
        return false;
    for (size_t i = 0; i < runtime->params_len; i++) {
        if (runtime->param_names[i] && strcmp(runtime->param_names[i], name) == 0) {
            *out_index = i;
            return true;
        }
    }
    return false;
}

static bool test_runtime_audio_port_index_by_name(
    const apg_v2_registry_audio_port_t *ports, size_t ports_len, const char *port_name, size_t *out_index
) {
    if (!ports || !port_name || !out_index)
        return false;
    for (size_t i = 0; i < ports_len; i++) {
        if (ports[i].port_name && strcmp(ports[i].port_name, port_name) == 0) {
            *out_index = i;
            return true;
        }
    }
    return false;
}

static bool
test_runtime_input_audio_port_index_by_name(const apg_v2_runtime_t *runtime, const char *port_name, size_t *out_index) {
    return runtime ? test_runtime_audio_port_index_by_name(
                         runtime->input_audio_ports, runtime->input_audio_ports_len, port_name, out_index
                     )
                   : false;
}

static bool test_runtime_output_audio_port_index_by_name(
    const apg_v2_runtime_t *runtime, const char *port_name, size_t *out_index
) {
    return runtime ? test_runtime_audio_port_index_by_name(
                         runtime->output_audio_ports, runtime->output_audio_ports_len, port_name, out_index
                     )
                   : false;
}

static bool test_runtime_set_param_by_name(apg_v2_runtime_t *runtime, const char *name, float value) {
    size_t index = 0u;
    return test_runtime_param_index_by_name(runtime, name, &index) &&
           apg_v2_runtime_set_param_index(runtime, index, value);
}

static bool test_runtime_set_control_port_by_name(apg_v2_runtime_t *runtime, const char *port_name, float value) {
    if (!runtime || !port_name)
        return false;
    for (size_t i = 0; i < runtime->control_targets_len; i++) {
        const apg_v2_registry_control_target_t *target = &runtime->control_targets[i];
        if (target->port_name && strcmp(target->port_name, port_name) == 0)
            return apg_v2_runtime_set_control_port_index(runtime, i, value);
    }
    return false;
}

static bool test_runtime_set_instance_bypass_by_name(apg_v2_runtime_t *runtime, const char *instance_id, bool enabled) {
    if (!runtime || !instance_id)
        return false;
    for (size_t i = 0; i < runtime->bypassed_instances_len; i++) {
        const apg_v2_runtime_bypass_entry_t *entry = &runtime->bypassed_instances[i];
        if (!entry->instance_id || entry->instance_id_len != strlen(instance_id))
            continue;
        if (strncmp(entry->instance_id, instance_id, entry->instance_id_len) == 0)
            return apg_v2_runtime_set_instance_bypass_index(runtime, i, enabled);
    }
    return false;
}

static bool test_runtime_process_mono_ports(
    apg_v2_runtime_t *runtime,
    const char       *input_port_name,
    const float      *input,
    const char       *output_port_name,
    float            *output,
    uint32_t          frames
) {
    size_t input_index  = 0u;
    size_t output_index = 0u;
    if (!test_runtime_input_audio_port_index_by_name(runtime, input_port_name, &input_index)) {
        apg_v2_runtime_set_error(runtime, "v2 runtime input audio port signal lookup failed");
        return false;
    }
    if (!test_runtime_output_audio_port_index_by_name(runtime, output_port_name, &output_index)) {
        apg_v2_runtime_set_error(runtime, "v2 runtime output audio port signal lookup failed");
        return false;
    }
    return apg_v2_runtime_process_mono_port_indices(runtime, input_index, input, output_index, output, frames);
}

static bool test_runtime_process_interleaved_ports(
    apg_v2_runtime_t *runtime,
    const char       *input_port_name,
    const float      *input,
    const char       *output_port_name,
    float            *output,
    uint32_t          frames
) {
    size_t input_index  = 0u;
    size_t output_index = 0u;
    if (!test_runtime_input_audio_port_index_by_name(runtime, input_port_name, &input_index)) {
        apg_v2_runtime_set_error(runtime, "v2 runtime input audio port signal lookup failed");
        return false;
    }
    if (!test_runtime_output_audio_port_index_by_name(runtime, output_port_name, &output_index)) {
        apg_v2_runtime_set_error(runtime, "v2 runtime output audio port signal lookup failed");
        return false;
    }
    return apg_v2_runtime_process_interleaved_port_indices(runtime, input_index, input, output_index, output, frames);
}

// Keep runtime tests on the production registry-init boundary.
static uc_status test_apg_v2_runtime_init_registry(
    const apg_v2_compiled_unit_t *plan,
    uint32_t                      frame_capacity,
    float                         sample_rate,
    uc_arena                     *registry_arena,
    apg_v2_runtime_t             *runtime,
    uc_error                     *err
) {
    apg_v2_registry_t registry = {0};
    uc_status         status =
        apg_v2_registry_build_with_growth(plan, frame_capacity, sample_rate, registry_arena, &registry, err);
    return status == UC_OK ? apg_v2_runtime_init_from_registry(&registry, runtime, err) : status;
}

#endif // AUDIO_PLAYGROUND_TEST_RUNTIME_V2_HARNESS_H
