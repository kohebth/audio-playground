#ifndef AUDIO_PLAYGROUND_APGCORE_ATOM_COST_H
#define AUDIO_PLAYGROUND_APGCORE_ATOM_COST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <apgcore/compiler/compiler_v2.h>
#include <apgcore/runtime/process.h>
#include <apgcore/runtime/spectral.h>
#include <atom_registry.h>

#define APG_COST_MODEL_SCHEMA "apg.cost_model.v1"

typedef enum {
    APG_COST_TRIVIAL = 0,
    APG_COST_LOW,
    APG_COST_MEDIUM,
    APG_COST_HIGH,
    APG_COST_EXTREME,
} apg_atom_cost_class_t;

typedef struct {
    uint64_t cpu_acu;
    uint64_t persistent_bytes;
    uint64_t scratch_bytes;
    uint32_t latency_frames;
    apg_atom_cost_class_t cost_class;
} apg_atom_cost_result_t;

typedef struct {
    uint64_t cpu_acu;
    uint64_t persistent_bytes;
    uint64_t scratch_bytes;
    uint32_t latency_frames;
    apg_atom_cost_class_t cost_class;
    size_t atom_count;
} apg_graph_cost_result_t;

/*
 * Estimate deterministic normalized work for one atom invocation.
 * ACU is a stable relative work unit, not elapsed time or a deadline guarantee.
 * A NULL config uses conservative defaults for configuration-dependent atoms.
 */
bool apg_atom_estimate_cost(
    const atom_registry_entry_t *entry,
    const void *config,
    const apg_process_info_t *process_info,
    const apg_spectral_info_t *spectral_info,
    apg_atom_cost_result_t *out
);

/* Sum CPU and persistent memory for scheduled atoms. Scratch memory is the maximum
 * simultaneously required scratch block. Latency is conservatively summed because
 * this API has no graph-edge topology; topology-aware callers may replace it with
 * longest-path latency calculation.
 */
bool apg_graph_estimate_cost(
    const atom_registry_entry_t *const *entries,
    const void *const *configs,
    const apg_spectral_info_t *const *spectral_infos,
    size_t count,
    const apg_process_info_t *process_info,
    apg_graph_cost_result_t *out
);

/*
 * Estimate a compiled unit in execution-schedule order. Configuration-dependent
 * atoms use conservative defaults; compiled spectral contexts are honored exactly.
 */
bool apg_compiled_unit_estimate_cost(
    const apg_v2_compiled_unit_t *compiled,
    const apg_process_info_t *process_info,
    apg_graph_cost_result_t *out
);

apg_atom_cost_class_t apg_cost_classify(uint64_t cpu_acu);
const char *apg_cost_class_name(apg_atom_cost_class_t cost_class);

#endif // AUDIO_PLAYGROUND_APGCORE_ATOM_COST_H
