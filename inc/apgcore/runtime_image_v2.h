#ifndef AUDIO_PLAYGROUND_APGCORE_RUNTIME_IMAGE_V2_H
#define AUDIO_PLAYGROUND_APGCORE_RUNTIME_IMAGE_V2_H

#include <stddef.h>
#include <stdint.h>

#include <apgcore/compiler_v2.h>
#include <yaml/arena.h>
#include <yaml/error.h>

typedef struct {
    const char *port_name;
    const char *param_name;
    size_t      param_index;
} apg_v2_runtime_control_target_t;

typedef struct {
    size_t out_size;
    size_t in_size;
    size_t config_size;
    size_t state_size;
    size_t state_buffers_len;
    size_t state_buffer_samples;
} apg_v2_runtime_node_layout_t;

typedef struct {
    const apg_v2_compiled_unit_t    *plan;
    uint32_t                         frame_capacity;
    float                            sample_rate;
    size_t                           signals_len;
    size_t                           signal_samples;
    size_t                           params_len;
    float                           *param_defaults;
    size_t                           input_meters_len;
    size_t                           output_meters_len;
    apg_v2_runtime_control_target_t *control_targets;
    size_t                           control_targets_len;
    apg_v2_runtime_node_layout_t    *node_layouts;
    size_t                           nodes_len;
    size_t                           schedule_len;
    size_t                           state_buffers_len;
    size_t                           state_buffer_samples;
} apg_v2_runtime_image_t;

/*
 * Build a compact runtime layout descriptor from a compiled plan.
 * The descriptor is arena-owned and does not allocate audio/runtime buffers.
 */
uc_status apg_v2_runtime_image_build(
    const apg_v2_compiled_unit_t *plan,
    uint32_t                      frame_capacity,
    float                         sample_rate,
    uc_arena                     *arena,
    apg_v2_runtime_image_t       *out,
    uc_error                     *err
);

#endif // AUDIO_PLAYGROUND_APGCORE_RUNTIME_IMAGE_V2_H
