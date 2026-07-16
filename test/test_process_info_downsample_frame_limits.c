#include "test_atom_basic_common.h"
#include <limits.h>

static int test_src_rate_change_edge_cases(void) {
    float input[9] = {0.0f, 1.0f, 2.0f, NAN, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
    float output[10];
    for (size_t i = 0; i < sizeof(output) / sizeof(output[0]); ++i)
        output[i] = -99.0f;

    src_downsample_out_t    down_out     = {.signal = output};
    src_downsample_in_t     down_in      = {.signal = input};
    src_downsample_params_t down_params  = {.factor = 2};
    src_downsample_state_t  down_state   = {0};
    apg_stream_context_t    down_context = {
           .input_frames = 9u, .output_capacity = 3u, .sample_rate = 48000.0f, .sample_position = 0u
    };
    apg_stream_result_t result = src_downsample_process(&down_out, &down_in, &down_params, &down_state, &down_context);
    if (result.consumed_frames != 6u || result.produced_frames != 3u || output[0] != 0.0f || output[1] != 2.0f ||
        output[2] != 4.0f || output[3] != -99.0f)
        return fail("src_downsample_process did not respect output capacity");

    for (size_t i = 0; i < sizeof(output) / sizeof(output[0]); ++i)
        output[i] = -99.0f;
    down_params.factor           = 0;
    down_state.phase             = 0u;
    down_context.input_frames    = 4u;
    down_context.output_capacity = 4u;
    result = src_downsample_process(&down_out, &down_in, &down_params, &down_state, &down_context);
    if (result.consumed_frames != 4u || result.produced_frames != 4u || output[0] != 0.0f || output[1] != 1.0f ||
        output[2] != 2.0f || output[3] != 0.0f || output[4] != -99.0f)
        return fail("src_downsample_process did not clamp factor or sanitize input");

    for (size_t i = 0; i < sizeof(output) / sizeof(output[0]); ++i)
        output[i] = -99.0f;
    down_params.factor           = INT_MAX;
    down_state.phase             = 0u;
    down_context.input_frames    = 9u;
    down_context.output_capacity = 2u;
    result = src_downsample_process(&down_out, &down_in, &down_params, &down_state, &down_context);
    if (result.consumed_frames != 9u || result.produced_frames != 1u || output[0] != 0.0f || output[1] != -99.0f)
        return fail("src_downsample_process did not clamp extreme factor");

    float up_input[3] = {1.0f, NAN, 3.0f};
    for (size_t i = 0; i < sizeof(output) / sizeof(output[0]); ++i)
        output[i] = -99.0f;
    src_upsample_out_t    up_out     = {.signal = output};
    src_upsample_in_t     up_in      = {.signal = up_input};
    src_upsample_params_t up_params  = {.factor = INT_MAX};
    src_upsample_state_t  up_state   = {0};
    apg_stream_context_t  up_context = {
         .input_frames = 3u, .output_capacity = 5u, .sample_rate = 48000.0f, .sample_position = 0u
    };
    result = src_upsample_process(&up_out, &up_in, &up_params, &up_state, &up_context);
    if (result.consumed_frames != 1u || result.produced_frames != 5u || output[0] != 1.0f || output[1] != 0.0f ||
        output[2] != 0.0f || output[3] != 0.0f || output[4] != 0.0f || output[5] != -99.0f)
        return fail("src_upsample_process did not clamp factor or output capacity");

    for (size_t i = 0; i < sizeof(output) / sizeof(output[0]); ++i)
        output[i] = -99.0f;
    up_params.factor           = 2;
    up_state.phase             = 0u;
    up_context.output_capacity = 0u;
    result                     = src_upsample_process(&up_out, &up_in, &up_params, &up_state, &up_context);
    if (result.consumed_frames != 0u || result.produced_frames != 0u || output[0] != -99.0f)
        return fail("src_upsample_process accepted a zero output capacity");

    up_context.input_frames    = 0u;
    up_context.output_capacity = 5u;
    result                     = src_upsample_process(&up_out, &up_in, &up_params, &up_state, &up_context);
    if (result.consumed_frames != 0u || result.produced_frames != 0u || output[0] != -99.0f)
        return fail("src_upsample_process wrote for zero input frames");

    return 0;
}

static int test_src_phase_continuity(void) {
    float                   down_input_a[2] = {1.0f, 2.0f};
    float                   down_input_b[2] = {3.0f, 4.0f};
    float                   down_output[2]  = {-99.0f, -99.0f};
    src_downsample_out_t    down_out        = {.signal = down_output};
    src_downsample_in_t     down_in         = {.signal = down_input_a};
    src_downsample_params_t down_params     = {.factor = 3};
    src_downsample_state_t  down_state      = {0};
    apg_stream_context_t    context         = {
                   .input_frames = 2u, .output_capacity = 2u, .sample_rate = 48000.0f, .sample_position = 0u
    };

    apg_stream_result_t result = src_downsample_process(&down_out, &down_in, &down_params, &down_state, &context);
    if (result.consumed_frames != 2u || result.produced_frames != 1u || down_output[0] != 1.0f ||
        down_state.phase != 2u)
        return fail("src_downsample_process did not retain phase");
    down_in.signal          = down_input_b;
    context.sample_position = 2u;
    result                  = src_downsample_process(&down_out, &down_in, &down_params, &down_state, &context);
    if (result.consumed_frames != 2u || result.produced_frames != 1u || down_output[0] != 4.0f ||
        down_state.phase != 1u)
        return fail("src_downsample_process lost cross-block phase");

    float                 up_input[2]  = {5.0f, 6.0f};
    float                 up_output[4] = {-99.0f, -99.0f, -99.0f, -99.0f};
    src_upsample_out_t    up_out       = {.signal = up_output};
    src_upsample_in_t     up_in        = {.signal = up_input};
    src_upsample_params_t up_params    = {.factor = 3};
    src_upsample_state_t  up_state     = {0};
    context.output_capacity            = 2u;
    context.sample_position            = 0u;
    result                             = src_upsample_process(&up_out, &up_in, &up_params, &up_state, &context);
    if (result.consumed_frames != 1u || result.produced_frames != 2u || up_output[0] != 5.0f || up_output[1] != 0.0f ||
        up_state.phase != 1u)
        return fail("src_upsample_process did not retain pending phase");
    up_in.signal            = &up_input[result.consumed_frames];
    context.input_frames    = 1u;
    context.output_capacity = 2u;
    context.sample_position = result.consumed_frames;
    result                  = src_upsample_process(&up_out, &up_in, &up_params, &up_state, &context);
    if (result.consumed_frames != 1u || result.produced_frames != 2u || up_output[0] != 0.0f || up_output[1] != 6.0f ||
        up_state.phase != 2u)
        return fail("src_upsample_process lost cross-block phase");

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

        apg_stream_context_t context = {
            .input_frames    = (uint32_t)frames,
            .output_capacity = (uint32_t)(frames / 2),
            .sample_rate     = 48000.0f,
            .sample_position = 0u,
        };

        src_downsample_out_t    out    = {.signal = downsampled};
        src_downsample_in_t     in     = {.signal = signal};
        src_downsample_params_t params = {.factor = 2};
        src_downsample_state_t  state  = {0};
        apg_stream_result_t     result = src_downsample_process(&out, &in, &params, &state, &context);

        const int out_frames = frames / 2;
        if (result.consumed_frames != (uint32_t)frames || result.produced_frames != (uint32_t)out_frames)
            return fail("src_downsample_process returned incorrect stream counts");
        for (int i = 0; i < out_frames; i++) {
            float expected = signal[i * 2];
            if (fabsf(downsampled[i] - expected) > 1e-7f)
                return fail("src_downsample_process mismatch");
        }
        if (frames < 1024 && downsampled[out_frames] != -99.0f)
            return fail("src_downsample_process wrote past output capacity");

        float upsampled[2049];
        for (int i = 0; i < 2049; i++)
            upsampled[i] = -99.0f;

        context.output_capacity         = (uint32_t)(frames * 2);
        src_upsample_out_t    up_out    = {.signal = upsampled};
        src_upsample_in_t     up_in     = {.signal = signal};
        src_upsample_params_t up_params = {.factor = 2};
        src_upsample_state_t  up_state  = {0};
        result                          = src_upsample_process(&up_out, &up_in, &up_params, &up_state, &context);

        const int up_frames = frames * 2;
        if (result.consumed_frames != (uint32_t)frames || result.produced_frames != (uint32_t)up_frames)
            return fail("src_upsample_process returned incorrect stream counts");
        for (int i = 0; i < frames; i++) {
            if (fabsf(upsampled[i * 2] - signal[i]) > 1e-7f)
                return fail("src_upsample_process signal mismatch");
            if (fabsf(upsampled[i * 2 + 1]) > 1e-7f)
                return fail("src_upsample_process zero-fill mismatch");
        }
        if (upsampled[up_frames] != -99.0f)
            return fail("src_upsample_process wrote past output capacity");
    }

    if (test_src_phase_continuity())
        return 1;
    return test_src_rate_change_edge_cases();
}
