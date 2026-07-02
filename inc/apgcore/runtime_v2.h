#ifndef AUDIO_PLAYGROUND_APGCORE_RUNTIME_V2_H
#define AUDIO_PLAYGROUND_APGCORE_RUNTIME_V2_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <apgcore/compiler_v2.h>
#include <apgcore/process.h>
#include <apgcore/runtime_image_v2.h>
#include <atom_registry.h>
#include <yaml/arena.h>
#include <yaml/error.h>

typedef struct {
    atom_call_t                            call;
    void                                  *out_storage;
    void                                  *in_storage;
    void                                  *config_storage;
    void                                  *state_storage;
    float                                **state_buffers;
    size_t                                *state_buffer_samples;
    size_t                                 state_buffers_len;
    float                                **signal_array_pool;
    size_t                                 signal_array_pool_len;
    size_t                                 signal_array_pool_used;
    const apg_v2_runtime_signal_binding_t *signal_bindings;
    size_t                                 signal_bindings_len;
    const apg_v2_runtime_scalar_refresh_t *config_refreshes;
    size_t                                 config_refreshes_len;
    const apg_v2_runtime_scalar_refresh_t *input_refreshes;
    size_t                                 input_refreshes_len;
} apg_v2_runtime_node_t;

typedef struct {
    float    peak;
    float    rms;
    uint32_t frames;
    bool     valid;
} apg_v2_meter_snapshot_t;

typedef struct {
    const apg_v2_compiled_unit_t    *plan;
    apg_process_info_t               process_info;
    uint32_t                         frame_capacity;
    float                           *signal_pool;
    float                          **signals;
    size_t                           signals_len;
    float                           *params;
    float                           *param_defaults;
    float                           *param_targets;
    uint32_t                        *param_smoothing_remaining_frames;
    size_t                           params_len;
    bool                             has_processed;
    char                           **bypassed_instances;
    size_t                           bypassed_instances_len;
    apg_v2_runtime_control_target_t *control_targets;
    size_t                           control_targets_len;
    bool                             project_muted;
    bool                             project_soloed;
    float                          **signal_array_pool;
    size_t                           signal_array_pool_len;
    bool                             image_arena_ready;
    uc_arena                         image_arena;
    size_t                           input_meters_len;
    size_t                           output_meters_len;
    void                            *atom_storage_pool;
    size_t                           atom_storage_bytes;
    float                           *state_buffer_pool;
    size_t                           state_buffer_samples;
    apg_v2_runtime_node_t           *nodes;
    size_t                           nodes_len;
    char                             last_error[160];
} apg_v2_runtime_t;

/*
 * Initialize a runtime from a compiled plan and allocate owned signal, parameter, atom-call, and state buffers.
 * The compiled plan and its source unit must outlive the runtime. Call apg_v2_runtime_destroy when done.
 */
uc_status apg_v2_runtime_init(
    const apg_v2_compiled_unit_t *plan, uint32_t frame_capacity, float sample_rate, apg_v2_runtime_t *out, uc_error *err
);

/*
 * Production path: build a runtime image in the provided arena and initialize a runtime.
 * This returns a runtime with all stage-specific allocation complete and image metadata owned by runtime.
 */
uc_status apg_v2_runtime_init_from_plan(
    const apg_v2_compiled_unit_t *plan,
    uint32_t                      frame_capacity,
    float                         sample_rate,
    uc_arena                     *image_arena,
    apg_v2_runtime_t             *out,
    uc_error                     *err
);

/*
 * Initialize a runtime from a prebuilt runtime image descriptor.
 * The image and compiled plan must outlive the runtime.
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

/* Store project-level UI transport states; mute silences public output ports after processing. */
bool apg_v2_runtime_set_project_mute(apg_v2_runtime_t *runtime, bool muted);
bool apg_v2_runtime_set_project_solo(apg_v2_runtime_t *runtime, bool soloed);

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
