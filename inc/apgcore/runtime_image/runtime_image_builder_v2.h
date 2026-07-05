#ifndef AUDIO_PLAYGROUND_APGCORE_RUNTIME_IMAGE_BUILDER_V2_H
#define AUDIO_PLAYGROUND_APGCORE_RUNTIME_IMAGE_BUILDER_V2_H

#include <stdint.h>

#include <apgcore/compiler_v2.h>
#include <apgcore/runtime_image_v2.h>
#include <yaml/arena.h>
#include <yaml/error.h>

/*
 * Build a compact runtime layout descriptor from a compiled plan.
 * The descriptor is arena-owned and does not allocate audio/runtime buffers.
 */
uc_status apg_v2_runtime_image_build(
    const apg_v2_compiled_unit_t *plan,
    uint32_t                      frame_capacity,
    float                         sample_rate,
    uc_arena                     *arena,
    apg_v2_runtime_image_t       *out,
    uc_error                     *err
);

/*
 * Build a runtime layout descriptor, growing arena capacity as needed.
 * The caller owns both arena and descriptor storage; on success, out_arena
 * remains initialized and owns all image heap memory until freed by caller.
 */
uc_status apg_v2_runtime_image_build_with_growth(
    const apg_v2_compiled_unit_t *plan,
    uint32_t                      frame_capacity,
    float                         sample_rate,
    uc_arena                     *out_arena,
    apg_v2_runtime_image_t       *out_image,
    uc_error                     *err
);

#endif // AUDIO_PLAYGROUND_APGCORE_RUNTIME_IMAGE_BUILDER_V2_H
