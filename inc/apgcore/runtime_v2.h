#ifndef AUDIO_PLAYGROUND_APGCORE_RUNTIME_V2_H
#define AUDIO_PLAYGROUND_APGCORE_RUNTIME_V2_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <apgcore/process.h>
#include <apgcore/runtime_image_v2.h>
#include <yaml/error.h>

typedef struct apg_v2_runtime_node_t apg_v2_runtime_node_t;

typedef struct apg_v2_runtime_bypass_entry_t {
    const char *instance_id;
    size_t      instance_id_len;
    size_t      input_index;
    size_t      output_index;
    bool        enabled;
} apg_v2_runtime_bypass_entry_t;

typedef struct {
    float    peak;
    float    rms;
    uint32_t frames;
    bool     valid;
} apg_v2_meter_snapshot_t;

typedef struct {
    apg_process_info_t                 process_info;
    uint32_t                           frame_capacity;
    const char                       **signal_names;
    float                             *signal_pool;
    float                            **signals;
    size_t                             signals_len;
    const char                       **param_names;
    float                             *params;
    float                             *param_defaults;
    float                             *param_targets;
    const uint32_t                    *param_smoothing_frames;
    uint32_t                          *param_smoothing_remaining_frames;
    size_t                             params_len;
    bool                               has_processed;
    apg_v2_runtime_bypass_entry_t     *bypassed_instances;
    size_t                             bypassed_instances_len;
    size_t                            *bypass_index_by_node;
    apg_v2_runtime_control_target_t   *control_targets;
    size_t                             control_targets_len;
    bool                               project_muted;
    size_t                            *project_mute_output_indices;
    size_t                             project_mute_output_indices_len;
    const apg_v2_runtime_audio_port_t *input_audio_ports;
    size_t                             input_audio_ports_len;
    const apg_v2_runtime_audio_port_t *output_audio_ports;
    size_t                             output_audio_ports_len;
    float                            **signal_array_pool;
    size_t                             signal_array_pool_len;
    size_t                             input_meters_len;
    size_t                             output_meters_len;
    void                              *atom_storage_pool;
    size_t                             atom_storage_bytes;
    float                             *state_buffer_pool;
    size_t                             state_buffer_samples;
    apg_v2_runtime_node_t             *nodes;
    size_t                             nodes_len;
    const uint32_t                    *schedule;
    size_t                             schedule_len;
    char                               last_error[160];
} apg_v2_runtime_t;

/*
 * Initialize a runtime from a prebuilt runtime image descriptor.
 * The image metadata and borrowed source strings must outlive the runtime.
 */
uc_status apg_v2_runtime_init_from_image(const apg_v2_runtime_image_t *image, apg_v2_runtime_t *out, uc_error *err);

/* Return the mutable internal signal buffer for a graph signal name, or NULL if missing. */
float *apg_v2_runtime_find_signal(apg_v2_runtime_t *runtime, const char *name);

/* Return channel 0 for a named public audio input/output port, or NULL for non-audio or missing ports. */
float *apg_v2_runtime_find_input_port_signal(apg_v2_runtime_t *runtime, const char *port_name);
float *apg_v2_runtime_find_output_port_signal(apg_v2_runtime_t *runtime, const char *port_name);

/* Return the mapped signal buffer for an explicit public audio port channel, or NULL if out of range. */
float *
apg_v2_runtime_find_input_port_channel_signal(apg_v2_runtime_t *runtime, const char *port_name, size_t channel_index);
float *
apg_v2_runtime_find_output_port_channel_signal(apg_v2_runtime_t *runtime, const char *port_name, size_t channel_index);

/* Update a compiled parameter value used by config bindings on the next process call. */
bool apg_v2_runtime_set_param(apg_v2_runtime_t *runtime, const char *name, float value);

/* Update the parameter targeted by a public control input port. */
bool apg_v2_runtime_set_control_port(apg_v2_runtime_t *runtime, const char *port_name, float value);

/* Bypass a compiled project unit instance by copying its first external input signal to its first public output signal.
 */
bool apg_v2_runtime_set_instance_bypass(apg_v2_runtime_t *runtime, const char *instance_id, bool enabled);

/* Mute silences public output ports after processing. */
bool apg_v2_runtime_set_project_mute(apg_v2_runtime_t *runtime, bool muted);

/* Clear signal and state buffers and restore parameter defaults while preserving owned allocations. */
bool apg_v2_runtime_reset(apg_v2_runtime_t *runtime);

/* Execute the compiled schedule using internal graph signal buffers for frames <= frame_capacity. */
bool apg_v2_runtime_process(apg_v2_runtime_t *runtime, uint32_t frames);

/* Process interleaved external buffers through named public audio ports with any valid channel count. */
bool apg_v2_runtime_process_interleaved_ports(
    apg_v2_runtime_t *runtime,
    const char       *input_port_name,
    const float      *input,
    const char       *output_port_name,
    float            *output,
    uint32_t          frames
);

/* Process the first public mono input and output ports. */
bool apg_v2_runtime_process_mono(apg_v2_runtime_t *runtime, const float *input, float *output, uint32_t frames);

/* Process named public mono audio ports; rejects multi-channel ports. */
bool apg_v2_runtime_process_mono_ports(
    apg_v2_runtime_t *runtime,
    const char       *input_port_name,
    const float      *input,
    const char       *output_port_name,
    float            *output,
    uint32_t          frames
);

/* Free all runtime-owned allocations and zero the runtime structure. */
void apg_v2_runtime_destroy(apg_v2_runtime_t *runtime);

#endif // AUDIO_PLAYGROUND_APGCORE_RUNTIME_V2_H
