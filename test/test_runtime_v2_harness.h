#ifndef AUDIO_PLAYGROUND_TEST_RUNTIME_V2_HARNESS_H
#define AUDIO_PLAYGROUND_TEST_RUNTIME_V2_HARNESS_H

#include <apgcore/registry/registry_builder_v2.h>
#include <apgcore/runtime_v2.h>
#include <apgcore/runtime_v2_internal.h>

// Keep runtime tests on the production registry-init boundary.
static uc_status test_apg_v2_runtime_init_registry(
    const apg_v2_compiled_unit_t *plan,
    uint32_t                      frame_capacity,
    float                         sample_rate,
    uc_arena                     *registry_arena,
    apg_v2_runtime_t             *runtime,
    uc_error                     *err
) {
    apg_v2_registry_t registry = {0};
    uc_status         status =
        apg_v2_registry_build_with_growth(plan, frame_capacity, sample_rate, registry_arena, &registry, err);
    return status == UC_OK ? apg_v2_runtime_init_from_registry(&registry, runtime, err) : status;
}

#endif // AUDIO_PLAYGROUND_TEST_RUNTIME_V2_HARNESS_H
