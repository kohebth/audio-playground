#ifndef AUDIO_PLAYGROUND_APGCORE_COMPILER_V2_H
#define AUDIO_PLAYGROUND_APGCORE_COMPILER_V2_H

#include <stddef.h>
#include <stdint.h>

#include <apgcore/unit_v2.h>
#include <atom_registry.h>
#include <yaml/arena.h>
#include <yaml/error.h>

typedef enum {
    APG_BIND_SIGNAL,
    APG_BIND_PARAM,
    APG_BIND_LITERAL,
} apg_v2_binding_kind_t;

typedef struct {
    const char           *key;
    apg_v2_binding_kind_t kind;
    size_t                index;
    const char           *literal;
} apg_v2_compiled_binding_t;

typedef struct {
    const char                  *id;
    const atom_registry_entry_t *atom;
    apg_v2_compiled_binding_t   *in;
    size_t                       in_len;
    apg_v2_compiled_binding_t   *out;
    size_t                       out_len;
    apg_v2_compiled_binding_t   *config;
    size_t                       config_len;
} apg_v2_compiled_node_t;

typedef struct {
    const apg_unit_v2_t    *unit;
    uint32_t               *schedule;
    size_t                  schedule_len;
    apg_v2_compiled_node_t *nodes;
    size_t                  nodes_len;
    uint32_t               *signal_producers;
    size_t                  signal_producers_len;
} apg_v2_compiled_unit_t;

/*
 * Compile a loaded v2 unit into arena-owned atom metadata bindings and a topological schedule.
 * The input unit must outlive the compiled plan; both are usually allocated from the same arena.
 */
uc_status apg_v2_compile_unit(const apg_unit_v2_t *unit, uc_arena *arena, apg_v2_compiled_unit_t *out, uc_error *err);

#endif // AUDIO_PLAYGROUND_APGCORE_COMPILER_V2_H
