#ifndef AUDIO_PLAYGROUND_APGCORE_REGISTRY_V2_H
#define AUDIO_PLAYGROUND_APGCORE_REGISTRY_V2_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <apgcore/binding_v2.h>
#include <atom_registry.h>

typedef struct {
    const char *port_name;
    const char *param_name;
    size_t      param_index;
} apg_v2_registry_control_target_t;

typedef struct {
    const char *instance_id;
    size_t      instance_id_len;
    size_t      input_index;
    size_t      output_index;
} apg_v2_registry_bypass_entry_t;

typedef struct {
    const char *port_name;
    size_t      channel_count;
    size_t      meter_index;
    size_t     *signal_indices;
} apg_v2_registry_audio_port_t;

typedef struct {
    const char *key;
    size_t      storage_offset;
    size_t      signal_index;
    size_t     *signal_array_indices;
    size_t      signal_array_offset;
    size_t      signal_array_len;
    bool        is_input;
    bool        is_signal_array;
} apg_v2_registry_signal_binding_t;

typedef struct {
    const char           *key;
    apg_v2_binding_kind_t kind;
    size_t                param_index;
    float                 number;
    size_t                storage_offset;
    atom_field_type_t     field_type;
    bool                  config;
} apg_v2_registry_scalar_refresh_t;

typedef struct {
    const char                       *node_id;
    const char                       *atom_name;
    atom_thunk_fn                     thunk;
    const atom_field_desc_t          *state_fields;
    int                               n_state_fields;
    size_t                            out_size;
    size_t                            in_size;
    size_t                            config_size;
    size_t                            state_size;
    size_t                            out_offset;
    size_t                            in_offset;
    size_t                            config_offset;
    size_t                            state_offset;
    size_t                           *state_buffer_samples_by_index;
    size_t                           *state_buffer_sample_offsets_by_index;
    size_t                            state_buffers_len;
    size_t                            state_buffer_table_offset;
    size_t                            state_buffer_samples;
    size_t                            signal_array_pointer_slots;
    size_t                            signal_array_pool_offset;
    size_t                            signal_bindings_len;
    apg_v2_registry_signal_binding_t *signal_bindings;
    apg_v2_registry_scalar_refresh_t *config_refreshes;
    size_t                            config_refreshes_len;
    apg_v2_registry_scalar_refresh_t *input_refreshes;
    size_t                            input_refreshes_len;
    float                           **mix_matrix_row_pointers;
    float                            *mix_matrix_coefficients;
    size_t                            mix_matrix_coefficients_len;
    size_t                            mix_matrix_num_out;
    size_t                            mix_matrix_num_in;
} apg_v2_registry_node_layout_t;

typedef struct {
    uint32_t                          frame_capacity;
    float                             sample_rate;
    const char                      **signal_names;
    size_t                            signals_len;
    size_t                            signal_samples;
    const char                      **param_names;
    size_t                            params_len;
    float                            *param_defaults;
    uint32_t                         *param_smoothing_frames;
    size_t                            input_meters_len;
    size_t                            output_meters_len;
    apg_v2_registry_control_target_t *control_targets;
    size_t                            control_targets_len;
    apg_v2_registry_bypass_entry_t   *bypass_instances;
    size_t                            bypassed_instances_len;
    size_t                           *bypass_index_by_node;
    size_t                           *project_mute_output_indices;
    size_t                            project_mute_output_indices_len;
    apg_v2_registry_audio_port_t     *input_audio_ports;
    size_t                            input_audio_ports_len;
    apg_v2_registry_audio_port_t     *output_audio_ports;
    size_t                            output_audio_ports_len;
    apg_v2_registry_node_layout_t    *node_layouts;
    size_t                            nodes_len;
    const uint32_t                   *schedule;
    size_t                            schedule_len;
    size_t                            state_buffers_len;
    size_t                            state_buffer_samples;
    size_t                            atom_storage_bytes;
    size_t                            signal_array_pointer_slots;
} apg_v2_registry_t;

#endif // AUDIO_PLAYGROUND_APGCORE_REGISTRY_V2_H
