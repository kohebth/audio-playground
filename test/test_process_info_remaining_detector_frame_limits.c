#include "test_atom_basic_common.h"

int test_process_info_remaining_detector_frame_limits(void) {
    const int frame_sizes[] = {64, 128, 256, 512, 1024};

    for (size_t c = 0; c < sizeof(frame_sizes) / sizeof(frame_sizes[0]); c++) {
        const int frames = frame_sizes[c];
        float     x[1024];
        float     y[1024];
        float     buffer[4096];
        float     autocorr_buffer[1024];
        float     pitch_buffer[1024];

        for (int i = 0; i < 1024; i++) {
            x[i] = (float)(i + 1);
            y[i] = -99.0f;
        }
        memset(buffer, 0, sizeof(buffer));
        memset(autocorr_buffer, 0, sizeof(autocorr_buffer));
        memset(pitch_buffer, 0, sizeof(pitch_buffer));

        apg_process_info_t info = {.sample_rate = 48000.0f, .frames = (uint32_t)frames, .channels = 1};

        detect_slope_out_t    slope_out = {.slope = y};
        detect_slope_in_t     slope_in  = {.signal = x};
        detect_slope_params_t slope_params;
        detect_slope_state_t  slope_state = {.prev_sample = 0.0f};
        detect_slope_process(&slope_out, &slope_in, &slope_params, &slope_state, &info);
        for (int i = 0; i < frames; i++) {
            if (fabsf(y[i] - 1.0f) > 1e-7f)
                return fail("detect_slope_process mismatch");
        }
        if (fabsf(slope_state.prev_sample - x[frames - 1]) > 1e-7f)
            return fail("detect_slope_process state mismatch");
        if (frames < 1024 && y[frames] != -99.0f)
            return fail("detect_slope_process wrote past info.frames");

        for (int i = 0; i < 1024; i++) {
            x[i] = (i % 2 == 0) ? 1.0f : -1.0f;
            y[i] = -99.0f;
        }
        detect_zero_crossing_out_t    zero_out = {.trigger = y};
        detect_zero_crossing_in_t     zero_in  = {.signal = x};
        detect_zero_crossing_params_t zero_params;
        detect_zero_crossing_state_t  zero_state = {.prev_sample = 0.0f};
        detect_zero_crossing_process(&zero_out, &zero_in, &zero_params, &zero_state, &info);
        for (int i = 0; i < frames; i++) {
            if (fabsf(y[i] - 1.0f) > 1e-7f)
                return fail("detect_zero_crossing_process mismatch");
        }
        if (fabsf(zero_state.prev_sample - x[frames - 1]) > 1e-7f)
            return fail("detect_zero_crossing_process state mismatch");
        if (frames < 1024 && y[frames] != -99.0f)
            return fail("detect_zero_crossing_process wrote past info.frames");

        for (int i = 0; i < 1024; i++) {
            x[i] = 1.0f;
            y[i] = -99.0f;
        }
        memset(buffer, 0, sizeof(buffer));
        detect_rms_out_t    rms_out    = {.level = y};
        detect_rms_in_t     rms_in     = {.signal = x};
        detect_rms_params_t rms_params = {.window_size = 4};
        detect_rms_state_t  rms_state  = {.buffer = buffer, .write_pos = 0, .sum = 0.0f};
        detect_rms_process(&rms_out, &rms_in, &rms_params, &rms_state, &info);
        for (int i = 0; i < frames; i++) {
            float expected = sqrtf((float)((i < 4) ? (i + 1) : 4) / 4.0f);
            if (fabsf(y[i] - expected) > 1e-7f)
                return fail("detect_rms_process mismatch");
        }
        if (rms_state.write_pos != frames % 4 || fabsf(rms_state.sum - 4.0f) > 1e-7f)
            return fail("detect_rms_process state mismatch");
        if (frames < 1024 && y[frames] != -99.0f)
            return fail("detect_rms_process wrote past info.frames");

        for (int i = 0; i < 1024; i++) {
            x[i] = 1.0f;
            y[i] = -99.0f;
        }
        memset(autocorr_buffer, 0, sizeof(autocorr_buffer));
        detect_autocorrelate_out_t    autocorr_out    = {.correlation = y};
        detect_autocorrelate_in_t     autocorr_in     = {.signal = x};
        detect_autocorrelate_params_t autocorr_params = {.max_lag = 8};
        detect_autocorrelate_state_t  autocorr_state  = {.buffer = autocorr_buffer, .write_pos = 0};
        detect_autocorrelate_process(&autocorr_out, &autocorr_in, &autocorr_params, &autocorr_state, &info);
        for (int k = 0; k < 8; k++) {
            float expected = (frames == 1024) ? (float)frames : (float)(frames - k);
            if (fabsf(y[k] - expected) > 1e-7f)
                return fail("detect_autocorrelate_process mismatch");
        }
        if (fabsf(y[8]) > 1e-7f)
            return fail("detect_autocorrelate_process did not clear tail");
        if (autocorr_state.write_pos != frames % 1024)
            return fail("detect_autocorrelate_process state mismatch");
        if (frames < 1024 && y[frames] != -99.0f)
            return fail("detect_autocorrelate_process wrote past info.frames");

        for (int i = 0; i < 1024; i++) {
            x[i] = 0.0f;
            y[i] = -99.0f;
        }
        memset(pitch_buffer, 0, sizeof(pitch_buffer));
        detect_pitch_out_t    pitch_out    = {.pitch = y};
        detect_pitch_in_t     pitch_in     = {.signal = x};
        detect_pitch_params_t pitch_params = {.max_lag = 128, .sample_rate = 48000.0f};
        detect_pitch_state_t  pitch_state  = {.buffer = pitch_buffer, .write_pos = 0};
        detect_pitch_process(&pitch_out, &pitch_in, &pitch_params, &pitch_state, &info);
        for (int i = 0; i < frames; i++) {
            if (fabsf(y[i]) > 1e-7f)
                return fail("detect_pitch_process quiet mismatch");
        }
        if (pitch_state.write_pos != frames % 1024)
            return fail("detect_pitch_process state mismatch");
        if (frames < 1024 && y[frames] != -99.0f)
            return fail("detect_pitch_process wrote past info.frames");
    }

    return 0;
}
