#include <apgcore/measure_v2.h>
#include <apgcore/runtime_v2.h>

#include <math.h>

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

bool apg_v2_measure_runtime_snapshot(const apg_v2_runtime_t *runtime, apg_v2_measure_runtime_snapshot_t *out) {
    if (!runtime || !out)
        return false;
    *out = (apg_v2_measure_runtime_snapshot_t){
        .frame_capacity    = apg_v2_runtime_frame_capacity(runtime),
        .sample_rate       = apg_v2_runtime_sample_rate(runtime),
        .signals_len       = apg_v2_runtime_signal_count(runtime),
        .params_len        = apg_v2_runtime_param_count(runtime),
        .nodes_len         = apg_v2_runtime_node_count(runtime),
        .input_meters_len  = apg_v2_runtime_input_meters_len(runtime),
        .output_meters_len = apg_v2_runtime_output_meters_len(runtime),
        .has_processed     = apg_v2_runtime_has_processed(runtime),
        .project_muted     = apg_v2_runtime_project_muted(runtime),
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
    if (!apg_v2_runtime_resolve_input_port_channel_signal(
            runtime, port_name, channel_index, &signal_index, &meter_index
        ) ||
        meter_index >= apg_v2_runtime_input_meters_len(runtime))
        return false;
    const float *signal = apg_v2_runtime_signal_buffer_at(runtime, signal_index);
    if (!apg_v2_runtime_has_processed(runtime) || !signal) {
        *out = (apg_v2_meter_snapshot_t){0};
        return true;
    }
    uint32_t frames = apg_v2_runtime_output_frames(runtime);
    *out            = meter_snapshot_from_signal(signal, frames);
    return true;
}

bool apg_v2_measure_get_output_meter(
    const apg_v2_runtime_t *runtime, const char *port_name, size_t channel_index, apg_v2_meter_snapshot_t *out
) {
    if (!runtime || !out)
        return false;
    size_t signal_index = 0u;
    size_t meter_index  = 0u;
    if (!apg_v2_runtime_resolve_output_port_channel_signal(
            runtime, port_name, channel_index, &signal_index, &meter_index
        ) ||
        meter_index >= apg_v2_runtime_output_meters_len(runtime))
        return false;
    const float *signal = apg_v2_runtime_signal_buffer_at(runtime, signal_index);
    if (!apg_v2_runtime_has_processed(runtime) || !signal) {
        *out = (apg_v2_meter_snapshot_t){0};
        return true;
    }
    uint32_t frames = apg_v2_runtime_output_frames(runtime);
    *out            = meter_snapshot_from_signal(signal, frames);
    return true;
}

const char *apg_v2_measure_last_error(const apg_v2_runtime_t *runtime) { return apg_v2_runtime_last_error(runtime); }
