#include "test_atom_basic_common.h"

int test_process_info_amplitude_state_frame_limits(void) {
    const int frame_sizes[] = {64, 128, 256, 512, 1024};

    for (size_t c = 0; c < sizeof(frame_sizes) / sizeof(frame_sizes[0]); c++) {
        const int frames = frame_sizes[c];
        float     x[1024];
        float     gate[1024];
        float     y[1024];

        for (int i = 0; i < 1024; i++) {
            x[i]    = (i % 4 == 0) ? -1.25f : ((i % 4 == 1) ? -0.25f : ((i % 4 == 2) ? 0.25f : 1.25f));
            gate[i] = (i == 0) ? 1.0f : 0.0f;
            y[i]    = -99.0f;
        }

        apg_process_info_t info = {.sample_rate = 48000.0f, .frames = (uint32_t)frames, .channels = 1};

        amplitude_clip_hard_out_t    hard_out    = {.signal = y};
        amplitude_clip_hard_in_t     hard_in     = {.signal = x};
        amplitude_clip_hard_params_t hard_params = {.threshold = 0.5f};
        amplitude_clip_hard_state_t  hard_state;
        amplitude_clip_hard_process(&hard_out, &hard_in, &hard_params, &hard_state, &info);
        for (int i = 0; i < frames; i++) {
            float expected = x[i];
            if (expected > hard_params.threshold)
                expected = hard_params.threshold;
            else if (expected < -hard_params.threshold)
                expected = -hard_params.threshold;
            if (fabsf(y[i] - expected) > 1e-7f)
                return fail("amplitude_clip_hard_process mismatch");
        }
        if (frames < 1024 && y[frames] != -99.0f)
            return fail("amplitude_clip_hard_process wrote past info.frames");

        for (int i = 0; i < 1024; i++) {
            x[i] = 0.25f;
            y[i] = -99.0f;
        }
        amplitude_accumulate_out_t    acc_out = {.signal = y};
        amplitude_accumulate_in_t     acc_in  = {.signal = x};
        amplitude_accumulate_params_t acc_params;
        amplitude_accumulate_state_t  acc_state = {.accumulator = 1.0f};
        amplitude_accumulate_process(&acc_out, &acc_in, &acc_params, &acc_state, &info);
        for (int i = 0; i < frames; i++) {
            float expected = 1.0f + 0.25f * (float)(i + 1);
            if (fabsf(y[i] - expected) > 1e-7f)
                return fail("amplitude_accumulate_process mismatch");
        }
        if (fabsf(acc_state.accumulator - (1.0f + 0.25f * (float)frames)) > 1e-7f)
            return fail("amplitude_accumulate_process state mismatch");
        if (frames < 1024 && y[frames] != -99.0f)
            return fail("amplitude_accumulate_process wrote past info.frames");

        for (int i = 0; i < 1024; i++) {
            x[i]    = 0.75f + 0.01f * (float)i;
            gate[i] = (i == 0) ? 1.0f : 0.0f;
            y[i]    = -99.0f;
        }
        amplitude_latch_out_t    latch_out    = {.signal = y};
        amplitude_latch_in_t     latch_in     = {.signal = x, .gate = gate};
        amplitude_latch_params_t latch_params = {.threshold = 0.5f};
        amplitude_latch_state_t  latch_state  = {.latched_value = 0.0f, .prev_gate = 0};
        amplitude_latch_process(&latch_out, &latch_in, &latch_params, &latch_state, &info);
        for (int i = 0; i < frames; i++) {
            if (fabsf(y[i] - 0.75f) > 1e-7f)
                return fail("amplitude_latch_process mismatch");
        }
        if (fabsf(latch_state.latched_value - 0.75f) > 1e-7f || latch_state.prev_gate != 0)
            return fail("amplitude_latch_process state mismatch");
        if (frames < 1024 && y[frames] != -99.0f)
            return fail("amplitude_latch_process wrote past info.frames");

        for (int i = 0; i < 1024; i++) {
            x[i] = (i % 2 == 0) ? 0.25f : -0.5f;
            y[i] = -99.0f;
        }
        amplitude_normalize_out_t    norm_out    = {.signal = y};
        amplitude_normalize_in_t     norm_in     = {.signal = x};
        amplitude_normalize_params_t norm_params = {.target_level = 1.0f, .mode = NORMALIZE_PEAK};
        amplitude_normalize_state_t  norm_state  = {.running_peak = 0.0f};
        amplitude_normalize_process(&norm_out, &norm_in, &norm_params, &norm_state, &info);
        for (int i = 0; i < frames; i++) {
            float expected = x[i] * 2.0f;
            if (fabsf(y[i] - expected) > 1e-7f)
                return fail("amplitude_normalize_process peak mismatch");
        }
        if (fabsf(norm_state.running_peak - 0.5f) > 1e-7f)
            return fail("amplitude_normalize_process peak state mismatch");
        if (frames < 1024 && y[frames] != -99.0f)
            return fail("amplitude_normalize_process peak wrote past info.frames");

        for (int i = 0; i < 1024; i++) {
            x[i] = 0.5f;
            y[i] = -99.0f;
        }
        norm_params.mode        = NORMALIZE_RMS;
        norm_state.running_peak = 0.0f;
        amplitude_normalize_process(&norm_out, &norm_in, &norm_params, &norm_state, &info);
        for (int i = 0; i < frames; i++) {
            if (fabsf(y[i] - 1.0f) > 1e-7f)
                return fail("amplitude_normalize_process rms mismatch");
        }
        if (fabsf(norm_state.running_peak - 0.5f) > 1e-7f)
            return fail("amplitude_normalize_process rms state mismatch");
        if (frames < 1024 && y[frames] != -99.0f)
            return fail("amplitude_normalize_process rms wrote past info.frames");
    }

    return 0;
}
