#ifndef AUDIO_PLAYGROUND_WASM_TOOLS_IMAGE_H
#define AUDIO_PLAYGROUND_WASM_TOOLS_IMAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <apgcore/registry/registry_v2.h>
#include <yaml/arena.h>
#include <yaml/error.h>

#define APG_WASM_IMAGE_VERSION 1u

bool apg_wasm_image_build(
    const apg_v2_registry_t *registry, uint64_t revision, unsigned char **out_data, size_t *out_size, uc_error *error
);

bool apg_wasm_image_hydrate(
    const unsigned char *data,
    size_t               size,
    uc_arena            *arena,
    apg_v2_registry_t   *out_registry,
    uint64_t            *out_revision,
    uc_error            *error
);

#endif // AUDIO_PLAYGROUND_WASM_TOOLS_IMAGE_H
