#include "test_atom_basic_common.h"
#include <limits.h>

static int test_src_rate_change_edge_cases(void) {
    float input[9] = {0.0f, 1.0f, 2.0f, NAN, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
    float output[10];
    for (size_t i = 0; i < sizeof(output) / sizeof(output[0]); ++i)
        output[i] = -99.0f;

    src_downsample_out_t    down_out    = {.signal = output};
    src_downsample_in_t     down_in     = {.signal = input};
    src_downsample_params_t down_params = {.factor = 2};
    src_downsample_state_t  down_state;
    apg_process_info_t      down_info = {.sample_rate = 48000.0f, .frames = 9u, .output_frames = 3u, .channels = 1u};
    src_downsample_process(&down_out, &down_in, &down_params, &down_state, &down_info);
    if (output[0] != 0.0f || output[1] != 2.0f || output[2] != 4.0f || output[3] != -99.0f)
        return fail("src_downsample_process did not respect output capacity");

    for (size_t i = 0; i < sizeof(output) / sizeof(output[0]); ++i)
        output[i] = -99.0f;
    down_params.factor      = 0;
    down_info.frames        = 4u;
    down_info.output_frames = 4u;
    src_downsample_process(&down_out, &down_in, &down_params, &down_state, &down_info);
    if (output[0] != 0.0f || output[1] != 1.0f || output[2] != 2.0f || output[3] != 0.0f || output[4] != -99.0f)
        return fail("src_downsample_process did not clamp factor or sanitize input");

    for (size_t i = 0; i < sizeof(output) / sizeof(output[0]); ++i)
        output[i] = -99.0f;
    down_params.factor      = INT_MAX;
    down_info.frames        = 9u;
    down_info.output_frames = 2u;
    src_downsample_process(&down_out, &down_in, &down_params, &down_state, &down_info);
    if (output[0] != 0.0f || output[1] != -99.0f)
        return fail("src_downsample_process did not clamp extreme factor");

    float up_input[3] = {1.0f, NAN, 3.0f};
    for (size_t i = 0; i < sizeof(output) / sizeof(output[0]); ++i)
        output[i] = -99.0f;
    src_upsample_out_t    up_out    = {.signal = output};
    src_upsample_in_t     up_in     = {.signal = up_input};
    src_upsample_params_t up_params = {.factor = INT_MAX};
    src_upsample_state_t  up_state;
    apg_process_info_t    up_info = {.sample_rate = 48000.0f, .frames = 3u, .output_frames = 5u, .channels = 1u};
    src_upsample_process(&up_out, &up_in, &up_params, &up_state, &up_info);
    if (output[0] != 1.0f || output[1] != 0.0f || output[2] != 0.0f || output[3] != 0.0f || output[4] != 0.0f ||
        output[5] != -99.0f)
        return fail("src_upsample_process did not clamp factor or output capacity");

    for (size_t i = 0; i < sizeof(output) / sizeof(output[0]); ++i)
        output[i] = -99.0f;
    up_params.factor      = 2;
    up_info.output_frames = 0u;
    src_upsample_process(&up_out, &up_in, &up_params, &up_state, &up_info);
    if (output[0] != -99.0f)
        return fail("src_upsample_process guessed ambiguous output capacity");

    up_info.frames        = 0u;
    up_info.output_frames = 5u;
    src_upsample_process(&up_out, &up_in, &up_params, &up_state, &up_info);
    if (output[0] != -99.0f)
        return fail("src_upsample_process wrote for zero input frames");

    return 0;
}

int test_process_info_downsample_frame_limits(void) {
    const int frame_sizes[] = {64, 128, 256, 512, 1024};

    for (size_t c = 0; c < sizeof(frame_sizes) / sizeof(frame_sizes[0]); c++) {
        const int frames = frame_sizes[c];
        float     signal[1024];
        float     downsampled[1024];

        for (int i = 0; i < 1024; i++) {
            signal[i]      = (float)i;
            downsampled[i] = -99.0f;
        }

        apg_process_info_t info = {.sample_rate = 48000.0f, .frames = (uint32_t)frames, .channels = 1};

        src_downsample_out_t    out    = {.signal = downsampled};
        src_downsample_in_t     in     = {.signal = signal};
        src_downsample_params_t params = {.factor = 2};
        src_downsample_state_t  state;
        src_downsample_process(&out, &in, &params, &state, &info);

        const int out_frames = frames / 2;
        for (int i = 0; i < out_frames; i++) {
            float expected = signal[i * 2];
            if (fabsf(downsampled[i] - expected) > 1e-7f)
                return fail("src_downsample_process mismatch");
        }
        if (frames < 1024 && downsampled[out_frames] != -99.0f)
            return fail("src_downsample_process wrote past info.frames");

        float upsampled[2049];
        for (int i = 0; i < 2049; i++)
            upsampled[i] = -99.0f;

        apg_process_info_t up_info = {
            .sample_rate   = 48000.0f,
            .frames        = (uint32_t)frames,
            .output_frames = (uint32_t)(frames * 2),
            .channels      = 1,
        };
        src_upsample_out_t    up_out    = {.signal = upsampled};
        src_upsample_in_t     up_in     = {.signal = signal};
        src_upsample_params_t up_params = {.factor = 2};
        src_upsample_state_t  up_state;
        src_upsample_process(&up_out, &up_in, &up_params, &up_state, &up_info);

        const int up_frames = frames * 2;
        for (int i = 0; i < frames; i++) {
            if (fabsf(upsampled[i * 2] - signal[i]) > 1e-7f)
                return fail("src_upsample_process signal mismatch");
            if (fabsf(upsampled[i * 2 + 1]) > 1e-7f)
                return fail("src_upsample_process zero-fill mismatch");
        }
        if (upsampled[up_frames] != -99.0f)
            return fail("src_upsample_process wrote past output_frames");
    }

    return test_src_rate_change_edge_cases();
}
