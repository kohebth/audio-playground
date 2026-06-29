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
} apg_v2_runtime_t;

uc_status apg_v2_runtime_init(
    const apg_v2_compiled_unit_t *plan, uint32_t frame_capacity, float sample_rate, apg_v2_runtime_t *out, uc_error *err
);

float *apg_v2_runtime_find_signal(apg_v2_runtime_t *runtime, const char *name);
bool   apg_v2_runtime_set_param(apg_v2_runtime_t *runtime, const char *name, float value);
bool   apg_v2_runtime_process(apg_v2_runtime_t *runtime, uint32_t frames);
bool   apg_v2_runtime_process_mono(apg_v2_runtime_t *runtime, const float *input, float *output, uint32_t frames);

void apg_v2_runtime_destroy(apg_v2_runtime_t *runtime);

#endif // AUDIO_PLAYGROUND_APGCORE_RUNTIME_V2_H
