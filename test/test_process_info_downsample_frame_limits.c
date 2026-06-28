#include "test_atom_basic_common.h"

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

    return 0;
}
