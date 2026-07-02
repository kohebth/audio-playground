#include <apgcore/measure_v2.h>

#include <stdlib.h>
#include <string.h>

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

static bool audio_port_meter_index(
    const apg_unit_v2_port_t *ports, size_t ports_len, const char *port_name, size_t channel_index, size_t *out_index
) {
    if (!ports || !port_name || !out_index)
        return false;
    size_t offset = 0u;
    for (size_t i = 0; i < ports_len; i++) {
        if (!ports[i].type || strcmp(ports[i].type, "audio") != 0)
            continue;
        size_t channels = 0u;
        if (!parse_port_channel_count(&ports[i], &channels))
            continue;
        if (ports[i].name && strcmp(ports[i].name, port_name) == 0) {
            if (channel_index >= channels)
                return false;
            *out_index = offset + channel_index;
            return true;
        }
        offset += channels;
    }
    return false;
}

bool apg_v2_measure_runtime_snapshot(const apg_v2_runtime_t *runtime, apg_v2_measure_runtime_snapshot_t *out) {
    if (!runtime || !out)
        return false;
    *out = (apg_v2_measure_runtime_snapshot_t){
        .plan              = runtime->plan,
        .frame_capacity    = runtime->frame_capacity,
        .sample_rate       = runtime->process_info.sample_rate,
        .signals_len       = runtime->signals_len,
        .params_len        = runtime->params_len,
        .nodes_len         = runtime->nodes_len,
        .input_meters_len  = runtime->input_meters_len,
        .output_meters_len = runtime->output_meters_len,
        .has_processed     = runtime->has_processed,
        .project_muted     = runtime->project_muted,
        .project_soloed    = runtime->project_soloed,
    };
    return true;
}

bool apg_v2_measure_get_input_meter(
    const apg_v2_runtime_t *runtime, const char *port_name, size_t channel_index, apg_v2_meter_snapshot_t *out
) {
    if (!runtime || !runtime->plan || !runtime->plan->unit || !out)
        return false;
    size_t index = 0u;
    if (!audio_port_meter_index(
            runtime->plan->unit->input_ports, runtime->plan->unit->input_ports_len, port_name, channel_index, &index
        ) ||
        index >= runtime->input_meters_len)
        return false;
    *out = runtime->input_meters ? runtime->input_meters[index] : (apg_v2_meter_snapshot_t){0};
    return true;
}

bool apg_v2_measure_get_output_meter(
    const apg_v2_runtime_t *runtime, const char *port_name, size_t channel_index, apg_v2_meter_snapshot_t *out
) {
    if (!runtime || !runtime->plan || !runtime->plan->unit || !out)
        return false;
    size_t index = 0u;
    if (!audio_port_meter_index(
            runtime->plan->unit->output_ports, runtime->plan->unit->output_ports_len, port_name, channel_index, &index
        ) ||
        index >= runtime->output_meters_len)
        return false;
    *out = runtime->output_meters ? runtime->output_meters[index] : (apg_v2_meter_snapshot_t){0};
    return true;
}

const char *apg_v2_measure_last_error(const apg_v2_runtime_t *runtime) {
    return runtime && runtime->last_error[0] ? runtime->last_error : NULL;
}
