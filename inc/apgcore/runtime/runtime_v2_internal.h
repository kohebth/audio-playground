#ifndef AUDIO_PLAYGROUND_APGCORE_RUNTIME_V2_INTERNAL_H
#define AUDIO_PLAYGROUND_APGCORE_RUNTIME_V2_INTERNAL_H

#include <apgcore/runtime/process.h>
#include <apgcore/runtime/runtime_v2.h>
#include <atom_registry.h>

typedef struct apg_v2_runtime_node_t         apg_v2_runtime_node_t;
typedef struct apg_v2_runtime_bypass_entry_t apg_v2_runtime_bypass_entry_t;

struct apg_v2_runtime_bypass_entry_t {
    const char *instance_id;
    size_t      instance_id_len;
    size_t      input_index;
    size_t      output_index;
    bool        enabled;
};

struct apg_v2_runtime_t {
    apg_process_context_t               process_context;
    uint32_t                            frame_capacity;
    const char                        **signal_names;
    float                              *signal_pool;
    float                             **signals;
    size_t                              signals_len;
    const char                        **param_names;
    float                              *params;
    float                              *param_defaults;
    float                              *param_targets;
    const uint32_t                     *param_smoothing_frames;
    uint32_t                           *param_smoothing_remaining_frames;
    size_t                              params_len;
    bool                                has_processed;
    apg_v2_runtime_bypass_entry_t      *bypassed_instances;
    size_t                              bypassed_instances_len;
    size_t                             *bypass_index_by_node;
    apg_v2_registry_control_target_t   *control_targets;
    size_t                              control_targets_len;
    bool                                project_muted;
    size_t                             *project_mute_output_indices;
    size_t                              project_mute_output_indices_len;
    const apg_v2_registry_audio_port_t *input_audio_ports;
    size_t                              input_audio_ports_len;
    const apg_v2_registry_audio_port_t *output_audio_ports;
    size_t                              output_audio_ports_len;
    float                             **signal_array_pool;
    size_t                              signal_array_pool_len;
    size_t                              input_meters_len;
    size_t                              output_meters_len;
    void                               *atom_storage_pool;
    size_t                              atom_storage_bytes;
    float                              *state_buffer_pool;
    size_t                              state_buffer_samples;
    float                             **state_buffer_ptrs;
    size_t                             *state_buffer_sample_counts;
    size_t                              state_buffer_count;
    apg_v2_runtime_node_t              *nodes;
    size_t                              nodes_len;
    const uint32_t                     *schedule;
    size_t                              schedule_len;
    char                                last_error[160];
};

struct apg_v2_runtime_node_t {
    atom_call_t                             call;
    void                                   *out_storage;
    void                                   *in_storage;
    void                                   *config_storage;
    void                                   *state_storage;
    float                                 **state_buffers;
    size_t                                 *state_buffer_samples;
    size_t                                  state_buffers_len;
    float                                 **signal_array_pool;
    size_t                                  signal_array_pool_len;
    size_t                                  signal_array_pool_used;
    const apg_v2_registry_signal_binding_t *signal_bindings;
    size_t                                  signal_bindings_len;
    const apg_v2_registry_scalar_refresh_t *config_refreshes;
    size_t                                  config_refreshes_len;
    apg_v2_registry_scalar_refresh_t       *config_refreshes_runtime;
    size_t                                  config_refreshes_runtime_len;
    const apg_v2_registry_scalar_refresh_t *input_refreshes;
    size_t                                  input_refreshes_len;
    apg_v2_registry_scalar_refresh_t       *input_refreshes_runtime;
    size_t                                  input_refreshes_runtime_len;
    const char                             *node_id;
    const char                             *atom_name;
    atom_thunk_fn                           thunk;
    const atom_field_desc_t                *state_fields;
    int                                     n_state_fields;
    size_t                                  state_size;
};

void apg_v2_runtime_set_error(apg_v2_runtime_t *runtime, const char *msg);
bool apg_v2_runtime_execution_metadata_ready(const apg_v2_runtime_t *runtime);
void apg_v2_runtime_advance_smoothed_params(apg_v2_runtime_t *runtime, uint32_t frames);
void apg_v2_runtime_apply_project_mute(apg_v2_runtime_t *runtime, uint32_t frames);
bool apg_v2_runtime_run_node(apg_v2_runtime_t *runtime, size_t node_index, uint32_t frames);

bool apg_v2_runtime_dispatch_process(apg_v2_runtime_t *runtime, uint32_t frames);
bool apg_v2_runtime_dispatch_process_interleaved_ports(
    apg_v2_runtime_t                   *runtime,
    const apg_v2_registry_audio_port_t *input_port,
    const float                        *input,
    const apg_v2_registry_audio_port_t *output_port,
    float                              *output,
    uint32_t                            frames
);
bool apg_v2_runtime_dispatch_process_mono_audio_ports(
    apg_v2_runtime_t                   *runtime,
    const apg_v2_registry_audio_port_t *input_port,
    const float                        *input,
    const apg_v2_registry_audio_port_t *output_port,
    float                              *output,
    uint32_t                            frames
);

#endif // AUDIO_PLAYGROUND_APGCORE_RUNTIME_V2_INTERNAL_H
