#ifndef AUDIO_PLAYGROUND_APGCORE_RUNTIME_IMAGE_V2_H
#define AUDIO_PLAYGROUND_APGCORE_RUNTIME_IMAGE_V2_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <apgcore/compiler_v2.h>
#include <atom_registry.h>
#include <yaml/arena.h>
#include <yaml/error.h>

typedef struct {
    const char *port_name;
    const char *param_name;
    size_t      param_index;
} apg_v2_runtime_control_target_t;

typedef struct {
    const char *instance_id;
    size_t      instance_id_len;
    size_t      input_index;
    size_t      output_index;
} apg_v2_runtime_image_bypass_entry_t;

typedef struct {
    const apg_v2_compiled_binding_t *binding;
    size_t                           storage_offset;
    size_t                           signal_index;
    size_t                           signal_array_offset;
    size_t                           signal_array_len;
    bool                             is_input;
    bool                             is_signal_array;
} apg_v2_runtime_signal_binding_t;

typedef struct {
    const apg_v2_compiled_binding_t *binding;
    const char                      *binding_key;
    size_t                           storage_offset;
    atom_field_type_t                field_type;
    bool                             config;
} apg_v2_runtime_scalar_refresh_t;

typedef struct {
    size_t                           out_size;
    size_t                           in_size;
    size_t                           config_size;
    size_t                           state_size;
    size_t                           out_offset;
    size_t                           in_offset;
    size_t                           config_offset;
    size_t                           state_offset;
    size_t                          *state_buffer_samples_by_index;
    size_t                          *state_buffer_sample_offsets_by_index;
    size_t                           state_buffers_len;
    size_t                           state_buffer_samples;
    size_t                           signal_array_pointer_slots;
    size_t                           signal_array_pool_offset;
    size_t                           signal_bindings_len;
    apg_v2_runtime_signal_binding_t *signal_bindings;
    apg_v2_runtime_scalar_refresh_t *config_refreshes;
    size_t                           config_refreshes_len;
    apg_v2_runtime_scalar_refresh_t *input_refreshes;
    size_t                           input_refreshes_len;
    float                          **mix_matrix_row_pointers;
    float                           *mix_matrix_coefficients;
    size_t                           mix_matrix_coefficients_len;
    size_t                           mix_matrix_num_out;
    size_t                           mix_matrix_num_in;
} apg_v2_runtime_node_layout_t;

typedef struct {
    const apg_v2_compiled_unit_t        *plan;
    uint32_t                             frame_capacity;
    float                                sample_rate;
    size_t                               signals_len;
    size_t                               signal_samples;
    size_t                               params_len;
    float                               *param_defaults;
    size_t                               input_meters_len;
    size_t                               output_meters_len;
    apg_v2_runtime_control_target_t     *control_targets;
    size_t                               control_targets_len;
    apg_v2_runtime_image_bypass_entry_t *bypass_instances;
    size_t                               bypassed_instances_len;
    size_t                              *bypass_index_by_node;
    size_t                              *project_mute_output_indices;
    size_t                               project_mute_output_indices_len;
    apg_v2_runtime_node_layout_t        *node_layouts;
    size_t                               nodes_len;
    size_t                               schedule_len;
    size_t                               state_buffers_len;
    size_t                               state_buffer_samples;
    size_t                               atom_storage_bytes;
    size_t                               signal_array_pointer_slots;
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
/*
 * Build a runtime layout descriptor, growing arena capacity as needed.
 * The caller owns both arena and descriptor storage; on success, out_arena
 * remains initialized and owns all image heap memory until freed by caller.
 */
uc_status apg_v2_runtime_image_build_with_growth(
    const apg_v2_compiled_unit_t *plan,
    uint32_t                      frame_capacity,
    float                         sample_rate,
    uc_arena                     *out_arena,
    apg_v2_runtime_image_t       *out_image,
    uc_error                     *err
);

#endif // AUDIO_PLAYGROUND_APGCORE_RUNTIME_IMAGE_V2_H
