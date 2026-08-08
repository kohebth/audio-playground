#ifndef AUDIO_PLAYGROUND_APGCORE_RUNTIME_V2_H
#define AUDIO_PLAYGROUND_APGCORE_RUNTIME_V2_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <apgcore/registry/registry_v2.h>
#include <apgcore/runtime/buffer.h>
#include <yaml/error.h>

typedef struct apg_v2_runtime_t apg_v2_runtime_t;

/*
 * Initialize a runtime from a prebuilt registry descriptor.
 * The registry metadata and borrowed source strings must outlive the runtime.
 */
uc_status apg_v2_runtime_init_from_registry(const apg_v2_registry_t *registry, apg_v2_runtime_t *out, uc_error *err);
/* Allocate and initialize a runtime from a prebuilt registry descriptor. */
uc_status apg_v2_runtime_create_from_registry(const apg_v2_registry_t *registry, apg_v2_runtime_t **out, uc_error *err);

apg_const_buffer_t apg_v2_runtime_signal_buffer_at(const apg_v2_runtime_t *runtime, size_t signal_index);
apg_buffer_t       apg_v2_runtime_signal_buffer_at_mut(apg_v2_runtime_t *runtime, size_t signal_index);

bool apg_v2_runtime_input_port_channel_signal_index(
    const apg_v2_runtime_t *runtime,
    size_t                  port_index,
    size_t                  channel_index,
    size_t                 *out_signal_index,
    size_t                 *out_meter_index
);
bool apg_v2_runtime_output_port_channel_signal_index(
    const apg_v2_runtime_t *runtime,
    size_t                  port_index,
    size_t                  channel_index,
    size_t                 *out_signal_index,
    size_t                 *out_meter_index
);

/* Update a compiled parameter value used by config bindings on the next process call. */
bool apg_v2_runtime_set_param_index(apg_v2_runtime_t *runtime, size_t index, float value);

/* Update the parameter targeted by a public control input port. */
bool apg_v2_runtime_set_control_port_index(apg_v2_runtime_t *runtime, size_t control_target_index, float value);

/* Bypass a compiled project unit instance by registry bypass entry index.
 */
bool apg_v2_runtime_set_instance_bypass_index(apg_v2_runtime_t *runtime, size_t bypass_index, bool enabled);

/* Mute silences public output ports after processing. */
bool apg_v2_runtime_set_project_mute(apg_v2_runtime_t *runtime, bool muted);

/* Clear signal and state buffers and restore parameter defaults while preserving owned allocations. */
bool apg_v2_runtime_reset(apg_v2_runtime_t *runtime);

/* Execute the compiled schedule using internal graph signal buffers for frames <= frame_capacity. */
bool apg_v2_runtime_process(apg_v2_runtime_t *runtime, uint32_t frames);

/* Process interleaved external buffers. View lengths/capacities count samples across all channels. */
bool apg_v2_runtime_process_interleaved_port_indices(
    apg_v2_runtime_t  *runtime,
    size_t             input_port_index,
    apg_const_buffer_t input,
    size_t             output_port_index,
    apg_buffer_t       output,
    uint32_t           frames
);

/* Process the first public mono input and output ports. */
bool apg_v2_runtime_process_mono(
    apg_v2_runtime_t *runtime, apg_const_buffer_t input, apg_buffer_t output, uint32_t frames
);

/* Process pre-resolved public mono audio port indices; rejects multi-channel ports. */
bool apg_v2_runtime_process_mono_port_indices(
    apg_v2_runtime_t  *runtime,
    size_t             input_port_index,
    apg_const_buffer_t input,
    size_t             output_port_index,
    apg_buffer_t       output,
    uint32_t           frames
);

/* Free all runtime-owned allocations and zero the runtime structure. */
void apg_v2_runtime_destroy(apg_v2_runtime_t *runtime);
/* Destroy a runtime and free the wrapper object. */
void apg_v2_runtime_destroy_owned(apg_v2_runtime_t **runtime);

#endif // AUDIO_PLAYGROUND_APGCORE_RUNTIME_V2_H
