#ifndef AUDIO_PLAYGROUND_APGCORE_REGISTRY_BUILDER_V2_H
#define AUDIO_PLAYGROUND_APGCORE_REGISTRY_BUILDER_V2_H

#include <apgcore/compiler/compiler_v2.h>
#include <apgcore/registry/registry_v2.h>
#include <apgcore/runtime/prepare.h>
#include <yaml/arena.h>
#include <yaml/error.h>

/*
 * Build a compact runtime layout descriptor from a compiled plan.
 * The descriptor is arena-owned and does not allocate audio/runtime buffers.
 */
uc_status apg_v2_registry_build(
    const apg_v2_compiled_unit_t *plan,
    const apg_prepare_context_t  *prepare_context,
    uc_arena                     *arena,
    apg_v2_registry_t            *out,
    uc_error                     *err
);

/*
 * Build a runtime layout descriptor, growing arena capacity as needed.
 * The caller owns both arena and descriptor storage. out_arena must be zeroed
 * or initialized. On success, its previous storage is freed and replaced with
 * the grown registry storage; on failure, its previous storage is preserved.
 */
uc_status apg_v2_registry_build_with_growth(
    const apg_v2_compiled_unit_t *plan,
    const apg_prepare_context_t  *prepare_context,
    uc_arena                     *out_arena,
    apg_v2_registry_t            *out_registry,
    uc_error                     *err
);

#endif // AUDIO_PLAYGROUND_APGCORE_REGISTRY_BUILDER_V2_H
