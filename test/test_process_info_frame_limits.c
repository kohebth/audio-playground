#include "test_atom_basic_common.h"

int test_process_info_frame_limits(void) {
    const int frame_sizes[] = {64, 128, 256, 512, 1024};

    for (size_t c = 0; c < sizeof(frame_sizes) / sizeof(frame_sizes[0]); c++) {
        const int    frames = frame_sizes[c];
        float        a[1024];
        float        b[1024];
        float        x[1024];
        float        y[1024];
        static float delay_buffer[192000];
        memset(delay_buffer, 0, sizeof(delay_buffer));

        for (int i = 0; i < 1024; i++) {
            a[i] = (float)i * 0.001f;
            b[i] = 0.5f;
            x[i] = (i == 0) ? 1.0f : 0.0f;
            y[i] = -99.0f;
        }

        apg_process_info_t info = {.sample_rate = 48000.0f, .frames = (uint32_t)frames, .channels = 1};

        amplitude_multiply_out_t    mult_out = {.signal = y};
        amplitude_multiply_in_t     mult_in  = {.signal_a = a, .signal_b = b};
        amplitude_multiply_params_t mult_params;
        amplitude_multiply_state_t  mult_state;
        amplitude_multiply_process(&mult_out, &mult_in, &mult_params, &mult_state, &info);

        for (int i = 0; i < frames; i++) {
            float expected = a[i] * b[i];
            if (fabsf(y[i] - expected) > 1e-7f)
                return fail("amplitude_multiply_process frame mismatch");
        }
        if (frames < 1024 && y[frames] != -99.0f)
            return fail("amplitude_multiply_process wrote past info.frames");

        for (int i = 0; i < 1024; i++)
            y[i] = -99.0f;
        amplitude_clip_soft_out_t    clip_out    = {.signal = y};
        amplitude_clip_soft_in_t     clip_in     = {.signal = a};
        amplitude_clip_soft_params_t clip_params = {.threshold = 0.8f, .curve = 1};
        amplitude_clip_soft_state_t  clip_state;
        amplitude_clip_soft_process(&clip_out, &clip_in, &clip_params, &clip_state, &info);
        if (assert_finite_buffer(y, frames, "amplitude_clip_soft_process"))
            return 1;
        if (frames < 1024 && y[frames] != -99.0f)
            return fail("amplitude_clip_soft_process wrote past info.frames");

        for (int i = 0; i < 1024; i++)
            y[i] = -99.0f;
        delay_line_out_t    delay_out    = {.signal = y};
        delay_line_in_t     delay_in     = {.signal = x};
        delay_line_params_t delay_params = {.length = 12};
        delay_line_state_t  delay_state  = {.buffer = delay_buffer, .write_pos = 0};
        delay_line_process(&delay_out, &delay_in, &delay_params, &delay_state, &info);
        if (frames > 12 && fabsf(y[12] - 1.0f) > 1e-6f)
            return fail("delay_line_process impulse mismatch");
        if (frames < 1024 && y[frames] != -99.0f)
            return fail("delay_line_process wrote past info.frames");

        for (int i = 0; i < 1024; i++)
            y[i] = -99.0f;
        filter_biquad_coefficients_out_t    bq_out    = {.signal = y};
        filter_biquad_coefficients_in_t     bq_in     = {.signal = x};
        filter_biquad_coefficients_params_t bq_params = {
            .b0 = 0.30f, .b1 = 0.30f, .b2 = 0.0f, .a1 = -0.40f, .a2 = 0.0f
        };
        filter_biquad_coefficients_state_t bq_state = {0};
        filter_biquad_coefficients_process(&bq_out, &bq_in, &bq_params, &bq_state, &info);
        if (assert_finite_buffer(y, frames, "filter_biquad_coefficients_process"))
            return 1;
        if (frames < 1024 && y[frames] != -99.0f)
            return fail("filter_biquad_coefficients_process wrote past info.frames");
    }

    return 0;
}
