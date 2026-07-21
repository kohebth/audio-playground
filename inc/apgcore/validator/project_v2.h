#ifndef AUDIO_PLAYGROUND_APGCORE_PROJECT_V2_H
#define AUDIO_PLAYGROUND_APGCORE_PROJECT_V2_H

#include <stdbool.h>
#include <stddef.h>

#include <apgcore/validator/unit_v2.h>
#include <apgcore/validator/value_v2.h>
#include <yaml/arena.h>
#include <yaml/error.h>

typedef struct {
    const char *id;
    const char *file;
} apg_project_v2_unit_ref_t;

typedef struct {
    const char    *key;
    apg_v2_value_t value;
} apg_project_v2_param_override_t;

typedef struct {
    const char                      *id;
    const char                      *unit;
    apg_project_v2_param_override_t *params;
    size_t                           params_len;
} apg_project_v2_node_t;

typedef struct {
    const char *from;
    const char *to;
} apg_project_v2_route_t;

typedef struct {
    const char *instance;
    bool        bypassed;
} apg_project_v2_scene_bypass_t;

typedef struct {
    const char                      *name;
    apg_project_v2_param_override_t *params;
    size_t                           params_len;
    apg_project_v2_scene_bypass_t   *bypass;
    size_t                           bypass_len;
} apg_project_v2_scene_t;

typedef struct {
    const char  *default_profile;
    const char **export_profiles;
    size_t       export_profiles_len;
} apg_project_v2_targets_t;

typedef struct {
    const char                *name;
    const char                *version;
    apg_project_v2_unit_ref_t *units;
    size_t                     units_len;
    apg_project_v2_node_t     *nodes;
    size_t                     nodes_len;
    apg_project_v2_route_t    *routes;
    size_t                     routes_len;
    apg_project_v2_scene_t    *scenes;
    size_t                     scenes_len;
    apg_project_v2_targets_t   targets;
} apg_project_v2_t;

typedef struct {
    const char   *id;
    const char   *file;
    const char   *resolved_path;
    apg_unit_v2_t unit;
} apg_project_v2_loaded_unit_t;

typedef struct {
    apg_project_v2_t              project;
    apg_project_v2_loaded_unit_t *units;
    size_t                        units_len;
} apg_project_v2_resolved_t;

/*
 * Parse a v2 project contract YAML document into an arena-owned syntax graph.
 * Parsing is syntax-only; semantic validation is handled by validator helpers.
 */
uc_status
apg_project_v2_parse_string(const char *src, size_t src_len, uc_arena *arena, uc_node **out_root, uc_error *err);
uc_status apg_project_v2_parse_file(const char *path, uc_arena *arena, uc_node **out_root, uc_error *err);

uc_status apg_project_v2_load_file(const char *path, uc_arena *arena, apg_project_v2_t *out, uc_error *err);
uc_status
apg_project_v2_load_string(const char *src, size_t src_len, uc_arena *arena, apg_project_v2_t *out, uc_error *err);
uc_status
apg_project_v2_load_resolved_file(const char *path, uc_arena *arena, apg_project_v2_resolved_t *out, uc_error *err);

#endif // AUDIO_PLAYGROUND_APGCORE_PROJECT_V2_H
