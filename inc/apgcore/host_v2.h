#ifndef AUDIO_PLAYGROUND_APGCORE_HOST_V2_H
#define AUDIO_PLAYGROUND_APGCORE_HOST_V2_H

#include <stdbool.h>
#include <stdint.h>

#include <apgcore/compiler_v2.h>
#include <apgcore/runtime_v2.h>
#include <apgcore/unit_v2.h>
#include <yaml/arena.h>
#include <yaml/error.h>

typedef struct {
    uc_arena               arena;
    apg_unit_v2_t          unit;
    apg_v2_compiled_unit_t plan;
    apg_v2_runtime_t       runtime;
    bool                   arena_ready;
    bool                   runtime_ready;
} apg_v2_host_unit_t;

uc_status apg_v2_host_load_file(
    const char *path, uint32_t frame_capacity, float sample_rate, apg_v2_host_unit_t *out, uc_error *err
);

bool apg_v2_host_set_param(apg_v2_host_unit_t *host, const char *name, float value);

bool apg_v2_host_process_mono_ports(
    apg_v2_host_unit_t *host,
    const char         *input_port_name,
    const float        *input,
    const char         *output_port_name,
    float              *output,
    uint32_t            frames
);

void apg_v2_host_destroy(apg_v2_host_unit_t *host);

#endif // AUDIO_PLAYGROUND_APGCORE_HOST_V2_H
