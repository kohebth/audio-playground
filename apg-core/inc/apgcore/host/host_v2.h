#ifndef AUDIO_PLAYGROUND_APGCORE_HOST_V2_H
#define AUDIO_PLAYGROUND_APGCORE_HOST_V2_H

#include <stdbool.h>
#include <stdint.h>

#include <apgcore/runtime/buffer.h>
#include <apgcore/runtime/prepare.h>
#include <apgcore/validator/project_v2.h>
#include <yaml/error.h>

typedef struct apg_v2_host_unit         apg_v2_host_unit_t;
typedef struct apg_v2_host_project      apg_v2_host_project_t;
typedef struct apg_v2_host_project_swap apg_v2_host_project_swap_t;

uc_status apg_v2_host_load_file(
    const char *path, const apg_prepare_context_t *prepare_context, apg_v2_host_unit_t **out, uc_error *err
);

bool        apg_v2_host_set_param(apg_v2_host_unit_t *host, const char *name, float value);
const char *apg_v2_host_last_error(const apg_v2_host_unit_t *host);

bool apg_v2_host_process_mono_ports(
    apg_v2_host_unit_t *host,
    const char         *input_port_name,
    apg_const_buffer_t  input,
    const char         *output_port_name,
    apg_buffer_t        output,
    uint32_t            frames
);

void apg_v2_host_destroy(apg_v2_host_unit_t *host);

uc_status apg_v2_host_project_load_file(
    const char *path, const apg_prepare_context_t *prepare_context, apg_v2_host_project_t **out, uc_error *err
);

bool        apg_v2_host_project_set_param(apg_v2_host_project_t *host, const char *name, float value);
bool        apg_v2_host_project_set_bypass(apg_v2_host_project_t *host, const char *instance_id, bool enabled);
bool        apg_v2_host_project_set_mute(apg_v2_host_project_t *host, bool muted);
const char *apg_v2_host_project_last_error(const apg_v2_host_project_t *host);

uc_status apg_v2_host_project_prepare_swap(
    apg_v2_host_project_t           *host,
    const apg_project_v2_resolved_t *project,
    apg_v2_host_project_swap_t     **out,
    uc_error                        *err
);
bool apg_v2_host_project_commit_swap(apg_v2_host_project_t *host, apg_v2_host_project_swap_t **swap);
void apg_v2_host_project_swap_destroy(apg_v2_host_project_swap_t **swap);

bool apg_v2_host_project_process_mono_ports(
    apg_v2_host_project_t *host,
    const char            *input_port_name,
    apg_const_buffer_t     input,
    const char            *output_port_name,
    apg_buffer_t           output,
    uint32_t               frames
);

void apg_v2_host_project_destroy(apg_v2_host_project_t *host);

#endif // AUDIO_PLAYGROUND_APGCORE_HOST_V2_H
