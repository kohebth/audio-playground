#include "test_atom_basic_common.h"

int test_process_info_control_frame_limits(void) {
    const int frame_sizes[] = {64, 128, 256, 512, 1024};

    for (size_t c = 0; c < sizeof(frame_sizes) / sizeof(frame_sizes[0]); c++) {
        const int frames = frame_sizes[c];
        float     x[1024];
        float     y[1024];

        for (int i = 0; i < 1024; i++) {
            x[i] = (i % 4 == 0) ? 0.80f : 0.20f;
            y[i] = -99.0f;
        }

        apg_process_context_t info = {.sample_rate = 48000.0f, .frames = (uint32_t)frames};

        detect_threshold_out_t    thresh_out    = {.gate = y};
        detect_threshold_in_t     thresh_in     = {.signal = x};
        detect_threshold_params_t thresh_params = {.threshold = 0.5f};
        detect_threshold_state_t  thresh_state;
        detect_threshold_process(&thresh_out, &thresh_in, &thresh_params, &thresh_state, &info);
        for (int i = 0; i < frames; i++) {
            float expected = (x[i] >= thresh_params.threshold) ? 1.0f : 0.0f;
            if (fabsf(y[i] - expected) > 1e-7f)
                return fail("detect_threshold_process mismatch");
        }
        if (frames < 1024 && y[frames] != -99.0f)
            return fail("detect_threshold_process wrote past info.frames");

        for (int i = 0; i < 1024; i++)
            y[i] = -99.0f;
        detect_envelope_out_t    env_out    = {.envelope = y};
        detect_envelope_in_t     env_in     = {.signal = x};
        detect_envelope_params_t env_params = {.attack = 0.005f, .release = 0.050f};
        detect_envelope_state_t  env_state  = {.prev_envelope = 0.0f};
        detect_envelope_process(&env_out, &env_in, &env_params, &env_state, &info);
        if (assert_finite_buffer(y, frames, "detect_envelope_process"))
            return 1;
        if (env_state.prev_envelope < 0.0f || env_state.prev_envelope > 1.0f)
            return fail("detect_envelope_process state out of range");
        if (frames < 1024 && y[frames] != -99.0f)
            return fail("detect_envelope_process wrote past info.frames");

        for (int i = 0; i < 1024; i++)
            y[i] = -99.0f;
        detect_peak_out_t    peak_out    = {.level = y};
        detect_peak_in_t     peak_in     = {.signal = x};
        detect_peak_params_t peak_params = {.attack = 0.005f, .release = 0.050f};
        detect_peak_state_t  peak_state  = {.prev_peak = 0.0f};
        detect_peak_process(&peak_out, &peak_in, &peak_params, &peak_state, &info);
        if (assert_finite_buffer(y, frames, "detect_peak_process"))
            return 1;
        if (peak_state.prev_peak < 0.0f || peak_state.prev_peak > 1.0f)
            return fail("detect_peak_process state out of range");
        if (frames < 1024 && y[frames] != -99.0f)
            return fail("detect_peak_process wrote past info.frames");

        for (int i = 0; i < 1024; i++)
            y[i] = -99.0f;
        amplitude_smooth_out_t    smooth_out    = {.signal = y};
        amplitude_smooth_in_t     smooth_in     = {.signal = x};
        amplitude_smooth_params_t smooth_params = {.attack = 0.005f, .release = 0.050f};
        amplitude_smooth_state_t  smooth_state  = {.prev_value = 0.0f};
        amplitude_smooth_process(&smooth_out, &smooth_in, &smooth_params, &smooth_state, &info);
        if (assert_finite_buffer(y, frames, "amplitude_smooth_process"))
            return 1;
        if (smooth_state.prev_value < 0.0f || smooth_state.prev_value > 1.0f)
            return fail("amplitude_smooth_process state out of range");
        if (frames < 1024 && y[frames] != -99.0f)
            return fail("amplitude_smooth_process wrote past info.frames");
    }

    return 0;
}
