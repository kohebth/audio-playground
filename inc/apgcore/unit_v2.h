#ifndef AUDIO_PLAYGROUND_APGCORE_UNIT_V2_H
#define AUDIO_PLAYGROUND_APGCORE_UNIT_V2_H

#include <stddef.h>

#include <yaml/arena.h>
#include <yaml/error.h>
#include <yaml/unit.h>

typedef struct {
    const char *name;
    const char *type;
    const char *default_value;
    const char *min_value;
    const char *max_value;
    const char *smoothing_ms;
} apg_unit_v2_param_t;

typedef struct {
    const char *name;
    const char *type;
    const char *channels;
} apg_unit_v2_port_t;

typedef struct {
    const char *key;
    uc_value    value;
} apg_unit_v2_binding_t;

typedef struct {
    const char            *id;
    const char            *atom;
    apg_unit_v2_binding_t *in;
    size_t                 in_len;
    apg_unit_v2_binding_t *out;
    size_t                 out_len;
    apg_unit_v2_binding_t *config;
    size_t                 config_len;
} apg_unit_v2_node_t;

typedef struct {
    const char          *name;
    const char          *version;
    apg_unit_v2_param_t *params;
    size_t               params_len;
    apg_unit_v2_port_t  *input_ports;
    size_t               input_ports_len;
    apg_unit_v2_port_t  *output_ports;
    size_t               output_ports_len;
    const char         **signals;
    size_t               signals_len;
    apg_unit_v2_node_t  *nodes;
    size_t               nodes_len;
} apg_unit_v2_t;

uc_status apg_unit_v2_load_file(const char *path, uc_arena *arena, apg_unit_v2_t *out, uc_error *err);
uc_status apg_unit_v2_load_string(const char *src, size_t src_len, uc_arena *arena, apg_unit_v2_t *out, uc_error *err);

#endif // AUDIO_PLAYGROUND_APGCORE_UNIT_V2_H
