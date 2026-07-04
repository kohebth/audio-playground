#include <apgcore/measure_v2.h>
#include <apgcore/runtime_v2_internal.h>

#include <math.h>
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

static bool signal_index_for_port_channel(
    const apg_v2_runtime_audio_port_t *ports,
    size_t                             ports_len,
    const char                        *port_name,
    size_t                             channel_index,
    size_t                            *out_signal_index,
    size_t                            *out_meter_index
) {
    if (!ports || !port_name || !out_signal_index || !out_meter_index)
        return false;
    for (size_t i = 0; i < ports_len; i++) {
        const apg_v2_runtime_audio_port_t *port = &ports[i];
        if (port->port_name && strcmp(port->port_name, port_name) == 0) {
            if (channel_index >= port->channel_count)
                return false;
            *out_signal_index = port->signal_indices[channel_index];
            *out_meter_index  = port->meter_index + channel_index;
            return true;
        }
    }
    return false;
}

// ?e32c0a91:start? measures runtime-owned signal buffers through image-derived audio port maps.
bool apg_v2_measure_runtime_snapshot(const apg_v2_runtime_t *runtime, apg_v2_measure_runtime_snapshot_t *out) {
    if (!runtime || !out)
        return false;
    *out = (apg_v2_measure_runtime_snapshot_t){
        .frame_capacity    = runtime->frame_capacity,
        .sample_rate       = runtime->process_info.sample_rate,
        .signals_len       = runtime->signals_len,
        .params_len        = runtime->params_len,
        .nodes_len         = runtime->nodes_len,
        .input_meters_len  = runtime->input_meters_len,
        .output_meters_len = runtime->output_meters_len,
        .has_processed     = runtime->has_processed,
        .project_muted     = runtime->project_muted,
    };
    return true;
}

bool apg_v2_measure_get_input_meter(
    const apg_v2_runtime_t *runtime, const char *port_name, size_t channel_index, apg_v2_meter_snapshot_t *out
) {
    if (!runtime || !out)
        return false;
    size_t signal_index = 0u;
    size_t meter_index  = 0u;
    if (!signal_index_for_port_channel(
            runtime->input_audio_ports, runtime->input_audio_ports_len, port_name, channel_index, &signal_index,
            &meter_index
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
    if (!runtime || !out)
        return false;
    size_t signal_index = 0u;
    size_t meter_index  = 0u;
    if (!signal_index_for_port_channel(
            runtime->output_audio_ports, runtime->output_audio_ports_len, port_name, channel_index, &signal_index,
            &meter_index
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
// ?e32c0a91:end?

const char *apg_v2_measure_last_error(const apg_v2_runtime_t *runtime) {
    return runtime && runtime->last_error[0] ? runtime->last_error : NULL;
}
