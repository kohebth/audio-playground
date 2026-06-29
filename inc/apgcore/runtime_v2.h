#ifndef AUDIO_PLAYGROUND_APGCORE_RUNTIME_V2_H
#define AUDIO_PLAYGROUND_APGCORE_RUNTIME_V2_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <apgcore/compiler_v2.h>
#include <apgcore/process.h>
#include <atom_registry.h>
#include <yaml/error.h>

typedef struct {
    atom_call_t call;
    void       *out_storage;
    void       *in_storage;
    void       *config_storage;
    void       *state_storage;
    float     **state_buffers;
    size_t     *state_buffer_samples;
    size_t      state_buffers_len;
    void      **aux_blocks;
    size_t      aux_blocks_len;
} apg_v2_runtime_node_t;

typedef struct {
    const apg_v2_compiled_unit_t *plan;
    apg_process_info_t            process_info;
    uint32_t                      frame_capacity;
    float                        *signal_pool;
    float                       **signals;
    size_t                        signals_len;
    float                        *params;
    size_t                        params_len;
    apg_v2_runtime_node_t        *nodes;
    size_t                        nodes_len;
    char                          last_error[160];
} apg_v2_runtime_t;

/*
 * Initialize a runtime from a compiled plan and allocate owned signal, parameter, atom-call, and state buffers.
 * The compiled plan and its source unit must outlive the runtime. Call apg_v2_runtime_destroy when done.
 */
uc_status apg_v2_runtime_init(
    const apg_v2_compiled_unit_t *plan, uint32_t frame_capacity, float sample_rate, apg_v2_runtime_t *out, uc_error *err
);

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

/* Return the last runtime processing/setup error string, or NULL if no runtime error is currently recorded. */
const char *apg_v2_runtime_last_error(const apg_v2_runtime_t *runtime);

/* Free all runtime-owned allocations and zero the runtime structure. */
void apg_v2_runtime_destroy(apg_v2_runtime_t *runtime);

#endif // AUDIO_PLAYGROUND_APGCORE_RUNTIME_V2_H
