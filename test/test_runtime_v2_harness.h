#ifndef AUDIO_PLAYGROUND_TEST_RUNTIME_V2_HARNESS_H
#define AUDIO_PLAYGROUND_TEST_RUNTIME_V2_HARNESS_H

#include <apgcore/runtime_image_builder_v2.h>
#include <apgcore/runtime_v2.h>

// ?c4f2a9b1:start? keeps runtime tests on the production image-init boundary.
static uc_status test_apg_v2_runtime_init_image(
    const apg_v2_compiled_unit_t *plan,
    uint32_t                      frame_capacity,
    float                         sample_rate,
    uc_arena                     *image_arena,
    apg_v2_runtime_t             *runtime,
    uc_error                     *err
) {
    apg_v2_runtime_image_t image = {0};
    uc_status              status =
        apg_v2_runtime_image_build_with_growth(plan, frame_capacity, sample_rate, image_arena, &image, err);
    return status == UC_OK ? apg_v2_runtime_init_from_image(&image, runtime, err) : status;
}
// ?c4f2a9b1:end?

#endif // AUDIO_PLAYGROUND_TEST_RUNTIME_V2_HARNESS_H
