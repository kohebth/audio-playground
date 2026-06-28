#include "test_atom_basic_common.h"

int test_process_info_remaining_filter_frame_limits(void) {
    const int frame_sizes[] = {64, 128, 256, 512, 1024};

    for (size_t c = 0; c < sizeof(frame_sizes) / sizeof(frame_sizes[0]); c++) {
        const int frames = frame_sizes[c];
        float     x[1024];
        float     y[1024];
        float     delay[1024];
        float     kernel[2] = {1.0f, 0.5f};
        float     long_buffer[48000];
        float     fir_buffer[1024];

        for (int i = 0; i < 1024; i++) {
            x[i]     = 1.0f;
            delay[i] = 8.0f;
            y[i]     = -99.0f;
        }
        memset(long_buffer, 0, sizeof(long_buffer));
        memset(fir_buffer, 0, sizeof(fir_buffer));

        apg_process_info_t info = {.sample_rate = 48000.0f, .frames = (uint32_t)frames, .channels = 1};

        filter_differentiate_out_t    diff_out = {.signal = y};
        filter_differentiate_in_t     diff_in  = {.signal = x};
        filter_differentiate_params_t diff_params;
        filter_differentiate_state_t  diff_state = {.prev_sample = 0.0f};
        filter_differentiate_process(&diff_out, &diff_in, &diff_params, &diff_state, &info);
        for (int i = 0; i < frames; i++) {
            float expected = (i == 0) ? 1.0f : 0.0f;
            if (fabsf(y[i] - expected) > 1e-7f)
                return fail("filter_differentiate_process mismatch");
        }
        if (fabsf(diff_state.prev_sample - 1.0f) > 1e-7f)
            return fail("filter_differentiate_process state mismatch");
        if (frames < 1024 && y[frames] != -99.0f)
            return fail("filter_differentiate_process wrote past info.frames");

        for (int i = 0; i < 1024; i++)
            y[i] = -99.0f;
        filter_dc_block_out_t    dc_out    = {.signal = y};
        filter_dc_block_in_t     dc_in     = {.signal = x};
        filter_dc_block_params_t dc_params = {.coefficient = 0.0f};
        filter_dc_block_state_t  dc_state  = {.prev_input = 0.0f, .prev_output = 0.0f};
        filter_dc_block_process(&dc_out, &dc_in, &dc_params, &dc_state, &info);
        for (int i = 0; i < frames; i++) {
            float expected = (i == 0) ? 1.0f : 0.0f;
            if (fabsf(y[i] - expected) > 1e-7f)
                return fail("filter_dc_block_process mismatch");
        }
        if (fabsf(dc_state.prev_input - 1.0f) > 1e-7f || fabsf(dc_state.prev_output) > 1e-7f)
            return fail("filter_dc_block_process state mismatch");
        if (frames < 1024 && y[frames] != -99.0f)
            return fail("filter_dc_block_process wrote past info.frames");

        for (int i = 0; i < 1024; i++)
            y[i] = -99.0f;
        filter_integrate_out_t    int_out = {.signal = y};
        filter_integrate_in_t     int_in  = {.signal = x};
        filter_integrate_params_t int_params;
        filter_integrate_state_t  int_state = {.accumulator = 0.0f};
        filter_integrate_process(&int_out, &int_in, &int_params, &int_state, &info);
        float acc = 0.0f;
        for (int i = 0; i < frames; i++) {
            acc = 1.0f + 0.999f * acc;
            if (fabsf(y[i] - acc) > 1e-5f)
                return fail("filter_integrate_process mismatch");
        }
        if (fabsf(int_state.accumulator - acc) > 1e-5f)
            return fail("filter_integrate_process state mismatch");
        if (frames < 1024 && y[frames] != -99.0f)
            return fail("filter_integrate_process wrote past info.frames");

        for (int i = 0; i < 1024; i++)
            y[i] = -99.0f;
        memset(fir_buffer, 0, sizeof(fir_buffer));
        filter_fir_out_t    fir_out    = {.signal = y};
        filter_fir_in_t     fir_in     = {.signal = x};
        filter_fir_params_t fir_params = {.kernel = kernel, .kernel_size = 2};
        filter_fir_state_t  fir_state  = {.buffer = fir_buffer, .write_pos = 0};
        filter_fir_process(&fir_out, &fir_in, &fir_params, &fir_state, &info);
        for (int i = 0; i < frames; i++) {
            float expected = (i == 0) ? 1.0f : 1.5f;
            if (fabsf(y[i] - expected) > 1e-7f)
                return fail("filter_fir_process mismatch");
        }
        if (fir_state.write_pos != frames % 1024)
            return fail("filter_fir_process state mismatch");
        if (frames < 1024 && y[frames] != -99.0f)
            return fail("filter_fir_process wrote past info.frames");

        for (int i = 0; i < 1024; i++)
            y[i] = -99.0f;
        memset(long_buffer, 0, sizeof(long_buffer));
        filter_comb_ff_out_t    comb_ff_out    = {.signal = y};
        filter_comb_ff_in_t     comb_ff_in     = {.signal = x};
        filter_comb_ff_params_t comb_ff_params = {.delay_samples = 2048, .coefficient = 0.5f};
        filter_comb_ff_state_t  comb_ff_state  = {.buffer = long_buffer, .write_pos = 0};
        filter_comb_ff_process(&comb_ff_out, &comb_ff_in, &comb_ff_params, &comb_ff_state, &info);
        for (int i = 0; i < frames; i++) {
            if (fabsf(y[i] - 1.0f) > 1e-7f || fabsf(long_buffer[i] - 1.0f) > 1e-7f)
                return fail("filter_comb_ff_process mismatch");
        }
        if (comb_ff_state.write_pos != frames % 48000)
            return fail("filter_comb_ff_process state mismatch");
        if (frames < 1024 && y[frames] != -99.0f)
            return fail("filter_comb_ff_process wrote past info.frames");

        for (int i = 0; i < 1024; i++)
            y[i] = -99.0f;
        memset(long_buffer, 0, sizeof(long_buffer));
        filter_comb_fb_out_t    comb_fb_out    = {.signal = y};
        filter_comb_fb_in_t     comb_fb_in     = {.signal = x, .delay = NULL};
        filter_comb_fb_params_t comb_fb_params = {.delay_samples = 2048, .coefficient = 0.5f};
        filter_comb_fb_state_t  comb_fb_state  = {.buffer = long_buffer, .write_pos = 0};
        filter_comb_fb_process(&comb_fb_out, &comb_fb_in, &comb_fb_params, &comb_fb_state, &info);
        for (int i = 0; i < frames; i++) {
            if (fabsf(y[i] - 1.0f) > 1e-7f || fabsf(long_buffer[i] - 1.0f) > 1e-7f)
                return fail("filter_comb_fb_process mismatch");
        }
        if (comb_fb_state.write_pos != frames % 48000)
            return fail("filter_comb_fb_process state mismatch");
        if (frames < 1024 && y[frames] != -99.0f)
            return fail("filter_comb_fb_process wrote past info.frames");

        for (int i = 0; i < 1024; i++)
            y[i] = -99.0f;
        memset(long_buffer, 0, sizeof(long_buffer));
        filter_allpass_out_t    allpass_out    = {.signal = y};
        filter_allpass_in_t     allpass_in     = {.signal = x};
        filter_allpass_params_t allpass_params = {.delay_samples = 2048, .coefficient = 0.5f};
        filter_allpass_state_t  allpass_state  = {.buffer = long_buffer, .write_pos = 0};
        filter_allpass_process(&allpass_out, &allpass_in, &allpass_params, &allpass_state, &info);
        for (int i = 0; i < frames; i++) {
            if (fabsf(y[i] + 0.5f) > 1e-7f || fabsf(long_buffer[i] - 0.75f) > 1e-7f)
                return fail("filter_allpass_process mismatch");
        }
        if (allpass_state.write_pos != frames % 48000)
            return fail("filter_allpass_process state mismatch");
        if (frames < 1024 && y[frames] != -99.0f)
            return fail("filter_allpass_process wrote past info.frames");
    }

    return 0;
}
