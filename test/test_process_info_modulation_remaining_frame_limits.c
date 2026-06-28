#include "test_atom_basic_common.h"

int test_process_info_modulation_remaining_frame_limits(void) {
    const int frame_sizes[] = {64, 128, 256, 512, 1024};

    for (size_t c = 0; c < sizeof(frame_sizes) / sizeof(frame_sizes[0]); c++) {
        const int frames = frame_sizes[c];
        float     x[1024];
        float     mod[1024];
        float     pos[1024];
        float     source[2048];
        float     y[1024];
        float     fm_buffer[4096];
        float     phase_buffer[4096];

        for (int i = 0; i < 1024; i++) {
            x[i]   = (float)i * 0.01f;
            mod[i] = (i % 2 == 0) ? 0.5f : -0.25f;
            pos[i] = (float)(i % 256) + 0.5f;
            y[i]   = -99.0f;
        }
        for (int i = 0; i < 2048; i++)
            source[i] = (float)i * 0.01f;
        memset(fm_buffer, 0, sizeof(fm_buffer));
        memset(phase_buffer, 0, sizeof(phase_buffer));

        apg_process_info_t info = {.sample_rate = 48000.0f, .frames = (uint32_t)frames, .channels = 1};

        modulation_ring_out_t    ring_out = {.signal = y};
        modulation_ring_in_t     ring_in  = {.signal = x, .modulator = mod};
        modulation_ring_params_t ring_params;
        modulation_ring_state_t  ring_state;
        modulation_ring_process(&ring_out, &ring_in, &ring_params, &ring_state, &info);
        for (int i = 0; i < frames; i++) {
            float expected = x[i] * mod[i];
            if (fabsf(y[i] - expected) > 1e-7f)
                return fail("modulation_ring_process mismatch");
        }
        if (frames < 1024 && y[frames] != -99.0f)
            return fail("modulation_ring_process wrote past info.frames");

        for (int i = 0; i < 1024; i++)
            y[i] = -99.0f;
        modulation_frequency_out_t    fm_out    = {.signal = y};
        modulation_frequency_in_t     fm_in     = {.signal = x, .modulator = mod};
        modulation_frequency_params_t fm_params = {.depth = 0.0f};
        modulation_frequency_state_t  fm_state  = {.buffer = fm_buffer, .write_pos = 0, .current_delay = 0.0f};
        modulation_frequency_process(&fm_out, &fm_in, &fm_params, &fm_state, &info);
        for (int i = 0; i < frames; i++) {
            if (fabsf(y[i]) > 1e-7f)
                return fail("modulation_frequency_process expected zero-delay buffer output");
            if (fabsf(fm_buffer[i] - x[i]) > 1e-7f)
                return fail("modulation_frequency_process buffer mismatch");
        }
        if (fm_state.write_pos != frames % 4096 || fabsf(fm_state.current_delay) > 1e-7f)
            return fail("modulation_frequency_process state mismatch");
        if (frames < 1024 && y[frames] != -99.0f)
            return fail("modulation_frequency_process wrote past info.frames");

        for (int i = 0; i < 1024; i++)
            y[i] = -99.0f;
        modulation_phase_out_t    phase_out    = {.signal = y};
        modulation_phase_in_t     phase_in     = {.signal = x, .modulator = mod};
        modulation_phase_params_t phase_params = {.depth = 0.0f};
        modulation_phase_state_t  phase_state  = {.buffer = phase_buffer, .write_pos = 0};
        modulation_phase_process(&phase_out, &phase_in, &phase_params, &phase_state, &info);
        for (int i = 0; i < frames; i++) {
            if (fabsf(y[i]) > 1e-7f)
                return fail("modulation_phase_process expected zero-delay buffer output");
            if (fabsf(phase_buffer[i] - x[i]) > 1e-7f)
                return fail("modulation_phase_process buffer mismatch");
        }
        if (phase_state.write_pos != frames % 4096)
            return fail("modulation_phase_process state mismatch");
        if (frames < 1024 && y[frames] != -99.0f)
            return fail("modulation_phase_process wrote past info.frames");

        for (int i = 0; i < 1024; i++)
            y[i] = -99.0f;
        modulation_scrub_out_t    scrub_out    = {.signal = y};
        modulation_scrub_in_t     scrub_in     = {.buffer = source, .position = pos};
        modulation_scrub_params_t scrub_params = {.buffer_size = 2048};
        modulation_scrub_state_t  scrub_state;
        modulation_scrub_process(&scrub_out, &scrub_in, &scrub_params, &scrub_state, &info);
        for (int i = 0; i < frames; i++) {
            uint32_t idx      = (uint32_t)floorf(pos[i]);
            float    expected = source[idx] * 0.5f + source[idx + 1] * 0.5f;
            if (fabsf(y[i] - expected) > 1e-7f)
                return fail("modulation_scrub_process mismatch");
        }
        if (frames < 1024 && y[frames] != -99.0f)
            return fail("modulation_scrub_process wrote past info.frames");
    }

    return 0;
}
