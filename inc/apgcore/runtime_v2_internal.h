#ifndef AUDIO_PLAYGROUND_APGCORE_RUNTIME_V2_INTERNAL_H
#define AUDIO_PLAYGROUND_APGCORE_RUNTIME_V2_INTERNAL_H

#include <apgcore/runtime_v2.h>
#include <atom_registry.h>

struct apg_v2_runtime_bypass_entry_t {
    const char *instance_id;
    size_t      instance_id_len;
    size_t      input_index;
    size_t      output_index;
    bool        enabled;
};

struct apg_v2_runtime_node_t {
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
    const char                            *node_id;
    const char                            *atom_name;
    atom_thunk_fn                          thunk;
    const atom_field_desc_t               *state_fields;
    int                                    n_state_fields;
    size_t                                 state_size;
};

#endif // AUDIO_PLAYGROUND_APGCORE_RUNTIME_V2_INTERNAL_H
