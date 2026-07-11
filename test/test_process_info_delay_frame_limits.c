#include "test_atom_basic_common.h"

int test_process_info_delay_frame_limits(void) {
    const int frame_sizes[] = {64, 128, 256, 512, 1024};

    for (size_t c = 0; c < sizeof(frame_sizes) / sizeof(frame_sizes[0]); c++) {
        const int    frames = frame_sizes[c];
        float        x[1024];
        float        y[1024];
        static float buffer[192000];

        memset(buffer, 0, sizeof(buffer));
        for (int i = 0; i < 1024; i++) {
            x[i] = (float)(i + 1) * 0.01f;
            y[i] = -99.0f;
        }
        buffer[0] = 0.25f;
        buffer[7] = 0.80f;
        buffer[191999] = 0.60f;

        apg_process_info_t info = {.sample_rate = 48000.0f, .frames = (uint32_t)frames, .channels = 1};

        delay_unit_out_t    unit_out = {.signal = y};
        delay_unit_in_t     unit_in  = {.signal = x};
        delay_unit_params_t unit_params;
        delay_unit_state_t  unit_state = {.prev_sample = 0.25f};
        delay_unit_process(&unit_out, &unit_in, &unit_params, &unit_state, &info);
        if (fabsf(y[0] - 0.25f) > 1e-7f)
            return fail("delay_unit_process first sample mismatch");
        if (frames > 1 && fabsf(y[1] - x[0]) > 1e-7f)
            return fail("delay_unit_process delayed sample mismatch");
        if (fabsf(unit_state.prev_sample - x[frames - 1]) > 1e-7f)
            return fail("delay_unit_process state mismatch");
        if (frames < 1024 && y[frames] != -99.0f)
            return fail("delay_unit_process wrote past info.frames");

        for (int i = 0; i < 1024; i++)
            y[i] = -99.0f;
        delay_tap_feedback_out_t    feedback_out    = {.signal = y};
        delay_tap_feedback_in_t     feedback_in     = {.buffer = buffer, .tap_position = 7};
        delay_tap_feedback_params_t feedback_params = {.coefficient = 0.5f};
        delay_tap_feedback_state_t  feedback_state;
        delay_tap_feedback_process(&feedback_out, &feedback_in, &feedback_params, &feedback_state, &info);
        for (int i = 0; i < frames; i++) {
            if (fabsf(y[i] - 0.40f) > 1e-7f)
                return fail("delay_tap_feedback_process mismatch");
        }
        if (frames < 1024 && y[frames] != -99.0f)
            return fail("delay_tap_feedback_process wrote past info.frames");

        feedback_in.tap_position = -100;
        feedback_params.coefficient = NAN;
        delay_tap_feedback_process(&feedback_out, &feedback_in, &feedback_params, &feedback_state, &info);
        for (int i = 0; i < frames; ++i) {
            if (y[i] != 0.0f)
                return fail("delay_tap_feedback_process did not sanitize invalid tap/coefficient");
        }

        for (int i = 0; i < 1024; i++)
            y[i] = -99.0f;
        delay_tap_feedforward_out_t    feedforward_out    = {.signal = y};
        delay_tap_feedforward_in_t     feedforward_in     = {.buffer = buffer, .tap_position = 999999};
        delay_tap_feedforward_params_t feedforward_params = {.coefficient = 0.25f};
        delay_tap_feedforward_state_t  feedforward_state;
        delay_tap_feedforward_process(&feedforward_out, &feedforward_in, &feedforward_params, &feedforward_state, &info);
        for (int i = 0; i < frames; i++) {
            if (fabsf(y[i] - 0.15f) > 1e-7f)
                return fail("delay_tap_feedforward_process did not clamp tap position");
        }
        if (frames < 1024 && y[frames] != -99.0f)
            return fail("delay_tap_feedforward_process wrote past info.frames");

        memset(buffer, 0, sizeof(buffer));
        for (int i = 0; i < 1024; i++)
            y[i] = -99.0f;
        delay_fractional_out_t    frac_out    = {.signal = y};
        delay_fractional_in_t     frac_in     = {.signal = x};
        delay_fractional_params_t frac_params = {.delay_samples = 1.5f, .interpolation = INTERPOLATION_LINEAR};
        delay_fractional_state_t  frac_state  = {.buffer = buffer, .write_pos = 0};
        delay_fractional_process(&frac_out, &frac_in, &frac_params, &frac_state, &info);
        if (assert_finite_buffer(y, frames, "delay_fractional_process"))
            return 1;
        if (frac_state.write_pos != frames)
            return fail("delay_fractional_process write_pos mismatch");
        if (frames < 1024 && y[frames] != -99.0f)
            return fail("delay_fractional_process wrote past info.frames");
    }

    return 0;
}
