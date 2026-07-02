#include <apgcore/measure_v2.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>

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

static int signal_index_by_name(const apg_unit_v2_t *unit, const char *name) {
    if (!unit || !name)
        return -1;
    for (size_t i = 0; i < unit->signals_len; i++) {
        if (unit->signals[i] && strcmp(unit->signals[i], name) == 0)
            return (int)i;
    }
    return -1;
}

static const char *audio_port_channel_signal_name(const apg_unit_v2_port_t *port, size_t channel_index) {
    if (!port)
        return NULL;
    if (port->signals_len > 0u)
        return channel_index < port->signals_len ? port->signals[channel_index] : NULL;
    return channel_index == 0u ? port->name : NULL;
}

static bool signal_index_for_port_channel(
    const apg_unit_v2_t      *unit,
    const apg_unit_v2_port_t *ports,
    size_t                    ports_len,
    const char               *port_name,
    size_t                    channel_index,
    size_t                   *out_signal_index,
    size_t                   *out_meter_index
) {
    if (!unit || !ports || !port_name || !out_signal_index || !out_meter_index)
        return false;
    size_t meter_index = 0u;
    for (size_t i = 0; i < ports_len; i++) {
        const apg_unit_v2_port_t *port = &ports[i];
        if (!port->type || strcmp(port->type, "audio") != 0)
            continue;
        size_t channels = 0u;
        if (!parse_port_channel_count(port, &channels))
            continue;
        if (port->name && strcmp(port->name, port_name) == 0) {
            if (channel_index >= channels)
                return false;
            const char *signal_name = audio_port_channel_signal_name(port, channel_index);
            int         index       = signal_index_by_name(unit, signal_name);
            if (index < 0)
                return false;
            *out_signal_index = (size_t)index;
            *out_meter_index  = meter_index + channel_index;
            return true;
        }
        meter_index += channels;
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
    size_t signal_index = 0u;
    size_t meter_index  = 0u;
    if (!signal_index_for_port_channel(
            runtime->plan->unit, runtime->plan->unit->input_ports, runtime->plan->unit->input_ports_len, port_name,
            channel_index, &signal_index, &meter_index
        ) ||
        meter_index >= runtime->input_meters_len)
        return false;
    if (!runtime->has_processed || signal_index >= runtime->signals_len || !runtime->signals ||
        !runtime->signals[signal_index]) {
        *out = (apg_v2_meter_snapshot_t){0};
        return true;
    }
    uint32_t frames = runtime->process_info.output_frames;
    *out            = meter_snapshot_from_signal(runtime->signals[signal_index], frames);
    return true;
}

bool apg_v2_measure_get_output_meter(
    const apg_v2_runtime_t *runtime, const char *port_name, size_t channel_index, apg_v2_meter_snapshot_t *out
) {
    if (!runtime || !runtime->plan || !runtime->plan->unit || !out)
        return false;
    size_t signal_index = 0u;
    size_t meter_index  = 0u;
    if (!signal_index_for_port_channel(
            runtime->plan->unit, runtime->plan->unit->output_ports, runtime->plan->unit->output_ports_len, port_name,
            channel_index, &signal_index, &meter_index
        ) ||
        meter_index >= runtime->output_meters_len)
        return false;
    if (!runtime->has_processed || signal_index >= runtime->signals_len || !runtime->signals ||
        !runtime->signals[signal_index]) {
        *out = (apg_v2_meter_snapshot_t){0};
        return true;
    }
    uint32_t frames = runtime->process_info.output_frames;
    *out            = meter_snapshot_from_signal(runtime->signals[signal_index], frames);
    return true;
}

const char *apg_v2_measure_last_error(const apg_v2_runtime_t *runtime) {
    return runtime && runtime->last_error[0] ? runtime->last_error : NULL;
}
