#ifndef AUDIO_PLAYGROUND_APGCORE_COMPILER_V2_H
#define AUDIO_PLAYGROUND_APGCORE_COMPILER_V2_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <apgcore/compiler/binding_v2.h>
#include <apgcore/validator/unit_v2.h>
#include <atom_registry.h>
#include <yaml/arena.h>
#include <yaml/error.h>

typedef struct {
    const char           *key;
    apg_v2_binding_kind_t kind;
    size_t                index;
    const char           *literal;
    float                 number;
    size_t               *indices;
    size_t                indices_len;
    float                *numbers;
    size_t                rows;
    size_t                cols;
} apg_v2_compiled_binding_t;

typedef struct {
    const char                  *id;
    const atom_registry_entry_t *atom;
    const char                  *atom_name;
    atom_thunk_fn                thunk;
    size_t                       out_size;
    size_t                       in_size;
    size_t                       config_size;
    size_t                       state_size;
    const atom_field_desc_t     *input_fields;
    size_t                       input_fields_len;
    const atom_field_desc_t     *config_fields;
    size_t                       config_fields_len;
    const atom_field_desc_t     *state_fields;
    size_t                       state_fields_len;
    apg_v2_compiled_binding_t   *in;
    size_t                       in_len;
    apg_v2_compiled_binding_t   *out;
    size_t                       out_len;
    apg_v2_compiled_binding_t   *config;
    size_t                       config_len;
    apg_spectral_info_t          spectral_info;
    bool                         has_spectral_info;
} apg_v2_compiled_node_t;

typedef struct {
    const char *id;
    size_t      id_len;
    size_t      input_signal_index;
    size_t      output_signal_index;
    bool        bypassable;
} apg_v2_compiled_instance_t;

typedef struct {
    const apg_unit_v2_t        *unit;
    uint32_t                   *schedule;
    size_t                      schedule_len;
    apg_v2_compiled_node_t     *nodes;
    size_t                      nodes_len;
    uint32_t                   *signal_producers;
    size_t                      signal_producers_len;
    apg_v2_compiled_instance_t *instances;
    size_t                      instances_len;
    size_t                     *instance_index_by_node;
    size_t                      instance_index_by_node_len;
} apg_v2_compiled_unit_t;

/*
 * Compile a loaded v2 unit into arena-owned atom metadata bindings and a topological schedule.
 * The input unit must outlive the compiled plan; both are usually allocated from the same arena.
 */
uc_status apg_v2_compile_unit(const apg_unit_v2_t *unit, uc_arena *arena, apg_v2_compiled_unit_t *out, uc_error *err);

#endif // AUDIO_PLAYGROUND_APGCORE_COMPILER_V2_H
