#ifndef AUDIO_PLAYGROUND_APGCORE_MEASURE_V2_H
#define AUDIO_PLAYGROUND_APGCORE_MEASURE_V2_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <apgcore/runtime_v2.h>

typedef struct {
    uint32_t frame_capacity;
    float    sample_rate;
    size_t   signals_len;
    size_t   params_len;
    size_t   nodes_len;
    size_t   input_meters_len;
    size_t   output_meters_len;
    bool     has_processed;
    bool     project_muted;
    bool     project_soloed;
} apg_v2_measure_runtime_snapshot_t;

bool apg_v2_measure_runtime_snapshot(const apg_v2_runtime_t *runtime, apg_v2_measure_runtime_snapshot_t *out);
bool apg_v2_measure_get_input_meter(
    const apg_v2_runtime_t *runtime, const char *port_name, size_t channel_index, apg_v2_meter_snapshot_t *out
);
bool apg_v2_measure_get_output_meter(
    const apg_v2_runtime_t *runtime, const char *port_name, size_t channel_index, apg_v2_meter_snapshot_t *out
);
const char *apg_v2_measure_last_error(const apg_v2_runtime_t *runtime);

#endif // AUDIO_PLAYGROUND_APGCORE_MEASURE_V2_H
