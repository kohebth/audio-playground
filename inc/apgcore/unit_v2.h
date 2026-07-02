#ifndef AUDIO_PLAYGROUND_APGCORE_UNIT_V2_H
#define AUDIO_PLAYGROUND_APGCORE_UNIT_V2_H

#include <stddef.h>

#include <yaml/arena.h>
#include <yaml/error.h>
#include <yaml/unit.h>

typedef struct uc_node uc_node;

typedef struct {
    const char *title;
    const char *category;
    const char *description;
} apg_unit_v2_meta_t;

typedef struct {
    const char *name;
    const char *type;
    const char *default_value;
    const char *min_value;
    const char *max_value;
    const char *smoothing_ms;
    const char *ui_label;
    const char *ui_control;
    const char *ui_unit;
    const char *ui_scale;
    const char *ui_display_precision;
} apg_unit_v2_param_t;

typedef struct {
    const char  *name;
    const char  *type;
    const char  *channels;
    const char  *target_param;
    const char  *target_kind;
    const char  *target_name;
    const char **signals;
    size_t       signals_len;
} apg_unit_v2_port_t;

typedef struct {
    const char    *key;
    uc_value       value;
    const uc_node *node;
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
    const char *target;
    const char *supported;
} apg_unit_v2_compatibility_t;

typedef struct {
    const char                  *name;
    const char                  *version;
    apg_unit_v2_meta_t           meta;
    apg_unit_v2_param_t         *params;
    size_t                       params_len;
    apg_unit_v2_port_t          *input_ports;
    size_t                       input_ports_len;
    apg_unit_v2_port_t          *output_ports;
    size_t                       output_ports_len;
    const char                 **signals;
    size_t                       signals_len;
    apg_unit_v2_node_t          *nodes;
    size_t                       nodes_len;
    apg_unit_v2_compatibility_t *compatibility;
    size_t                       compatibility_len;
} apg_unit_v2_t;

/*
 * Parse a v2 unit contract YAML document into an arena-owned syntax graph.
 * This performs parsing only; validation and model materialization are done
 * by validator stage functions.
 */
uc_status apg_unit_v2_parse_string(const char *src, size_t src_len, uc_arena *arena, uc_node **out_root, uc_error *err);
uc_status apg_unit_v2_parse_file(const char *path, uc_arena *arena, uc_node **out_root, uc_error *err);

/*
 * Load a v2 unit from a YAML file into arena-owned storage.
 * The returned structure and all nested strings/arrays remain valid until the arena is freed.
 */
uc_status apg_unit_v2_load_file(const char *path, uc_arena *arena, apg_unit_v2_t *out, uc_error *err);

/*
 * Load a v2 unit from an in-memory YAML document into arena-owned storage.
 * src does not need to outlive the call; parsed data is copied or interned into the arena.
 */
uc_status apg_unit_v2_load_string(const char *src, size_t src_len, uc_arena *arena, apg_unit_v2_t *out, uc_error *err);

#endif // AUDIO_PLAYGROUND_APGCORE_UNIT_V2_H
