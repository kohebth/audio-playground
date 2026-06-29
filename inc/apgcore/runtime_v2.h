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

uc_status apg_v2_runtime_init(
    const apg_v2_compiled_unit_t *plan, uint32_t frame_capacity, float sample_rate, apg_v2_runtime_t *out, uc_error *err
);

float *apg_v2_runtime_find_signal(apg_v2_runtime_t *runtime, const char *name);
float *apg_v2_runtime_find_input_port_signal(apg_v2_runtime_t *runtime, const char *port_name);
float *apg_v2_runtime_find_output_port_signal(apg_v2_runtime_t *runtime, const char *port_name);
float *
apg_v2_runtime_find_input_port_channel_signal(apg_v2_runtime_t *runtime, const char *port_name, size_t channel_index);
float *
apg_v2_runtime_find_output_port_channel_signal(apg_v2_runtime_t *runtime, const char *port_name, size_t channel_index);
bool apg_v2_runtime_set_param(apg_v2_runtime_t *runtime, const char *name, float value);
bool apg_v2_runtime_set_control_port(apg_v2_runtime_t *runtime, const char *port_name, float value);
bool apg_v2_runtime_reset(apg_v2_runtime_t *runtime);
bool apg_v2_runtime_process(apg_v2_runtime_t *runtime, uint32_t frames);
bool apg_v2_runtime_process_interleaved_ports(
    apg_v2_runtime_t *runtime,
    const char       *input_port_name,
    const float      *input,
    const char       *output_port_name,
    float            *output,
    uint32_t          frames
);
bool apg_v2_runtime_process_mono(apg_v2_runtime_t *runtime, const float *input, float *output, uint32_t frames);
bool apg_v2_runtime_process_mono_ports(
    apg_v2_runtime_t *runtime,
    const char       *input_port_name,
    const float      *input,
    const char       *output_port_name,
    float            *output,
    uint32_t          frames
);
const char *apg_v2_runtime_last_error(const apg_v2_runtime_t *runtime);

void apg_v2_runtime_destroy(apg_v2_runtime_t *runtime);

#endif // AUDIO_PLAYGROUND_APGCORE_RUNTIME_V2_H
