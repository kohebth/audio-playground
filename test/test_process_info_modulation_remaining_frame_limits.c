#include "test_atom_basic_common.h"
#include <apgcore/dsp/dsp_safety.h>
#include <float.h>

static int test_modulation_delay_safety(void) {
    enum { BLOCK_FRAMES = 257, BLOCK_COUNT = 20 };
    float        input[BLOCK_FRAMES];
    float        modulator[BLOCK_FRAMES];
    float        output[BLOCK_FRAMES + 1];
    static float frequency_buffer[APG_MODULATION_DELAY_CAPACITY];
    static float phase_buffer[APG_MODULATION_DELAY_CAPACITY];

    for (size_t i = 0; i < BLOCK_FRAMES; ++i) {
        input[i]     = i == 3u ? NAN : sinf((float)i * 0.01f);
        modulator[i] = i % 3u == 0u ? FLT_MAX : (i % 3u == 1u ? -FLT_MAX : NAN);
    }
    for (size_t i = 0; i < APG_MODULATION_DELAY_CAPACITY; ++i) {
        frequency_buffer[i] = NAN;
        phase_buffer[i]     = NAN;
    }
    frequency_buffer[128] = phase_buffer[128] = -77.0f;

    apg_process_info_t info = {
        .sample_rate = 48000.0f, .frames = BLOCK_FRAMES, .output_frames = BLOCK_FRAMES, .channels = 1u
    };
    modulation_frequency_out_t    frequency_out    = {.signal = output};
    modulation_frequency_in_t     frequency_in     = {.signal = input, .modulator = modulator};
    modulation_frequency_params_t frequency_params = {.depth = FLT_MAX};
    modulation_frequency_state_t  frequency_state  = {
          .buffer = frequency_buffer, .buffer_len = 128u, .write_pos = -1, .current_delay = NAN
    };

    for (int block = 0; block < BLOCK_COUNT; ++block) {
        for (size_t i = 0; i < BLOCK_FRAMES + 1u; ++i)
            output[i] = -99.0f;
        frequency_params.depth = block % 2 == 0 ? FLT_MAX : -FLT_MAX;
        modulation_frequency_process(&frequency_out, &frequency_in, &frequency_params, &frequency_state, &info);
        if (assert_finite_buffer(output, BLOCK_FRAMES, "modulation_frequency_process extreme modulation"))
            return 1;
        if (output[BLOCK_FRAMES] != -99.0f)
            return fail("modulation_frequency_process wrote past info.frames");
        if (!isfinite(frequency_state.current_delay) || frequency_state.current_delay < 0.0f ||
            frequency_state.current_delay > 126.0f)
            return fail("modulation_frequency_process left invalid delay state");
    }
    const int expected_frequency_pos = (int)((127u + BLOCK_FRAMES * BLOCK_COUNT) % 128u);
    if (frequency_state.write_pos != expected_frequency_pos || frequency_buffer[128] != -77.0f)
        return fail("modulation_frequency_process did not preserve multi-block wrap state");

    modulation_phase_out_t    phase_out            = {.signal = output};
    modulation_phase_in_t     phase_in             = {.signal = input, .modulator = modulator};
    modulation_phase_params_t phase_params         = {.depth = -FLT_MAX};
    modulation_phase_state_t  phase_state          = {.buffer = phase_buffer, .buffer_len = 128u, .write_pos = -5000};
    const uint32_t            normalized_phase_pos = apg_wrap_index_i64(-5000, 128u);

    for (int block = 0; block < BLOCK_COUNT; ++block) {
        for (size_t i = 0; i < BLOCK_FRAMES + 1u; ++i)
            output[i] = -99.0f;
        phase_params.depth = block % 2 == 0 ? -FLT_MAX : FLT_MAX;
        modulation_phase_process(&phase_out, &phase_in, &phase_params, &phase_state, &info);
        if (assert_finite_buffer(output, BLOCK_FRAMES, "modulation_phase_process extreme modulation"))
            return 1;
        if (output[BLOCK_FRAMES] != -99.0f)
            return fail("modulation_phase_process wrote past info.frames");
    }
    const int expected_phase_pos = (int)((normalized_phase_pos + BLOCK_FRAMES * BLOCK_COUNT) % 128u);
    if (phase_state.write_pos != expected_phase_pos || phase_buffer[128] != -77.0f)
        return fail("modulation_phase_process did not preserve multi-block wrap state");

    output[0]             = -99.0f;
    info.frames           = 0u;
    phase_state.write_pos = -1;
    modulation_phase_process(&phase_out, &phase_in, &phase_params, &phase_state, &info);
    if (output[0] != -99.0f || phase_state.write_pos != 127)
        return fail("modulation_phase_process mishandled zero frames");

    return 0;
}

int test_process_info_modulation_remaining_frame_limits(void) {
    const int frame_sizes[] = {64, 128, 256, 512, 1024};

    for (size_t c = 0; c < sizeof(frame_sizes) / sizeof(frame_sizes[0]); c++) {
        const int frames = frame_sizes[c];
        float     x[1024];
        float     mod[1024];
        float     pos[1024];
        float     source[2048];
        float     y[1024];
        float     fm_buffer[APG_MODULATION_DELAY_CAPACITY];
        float     phase_buffer[APG_MODULATION_DELAY_CAPACITY];

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
        if (fm_state.write_pos != frames % (int)APG_MODULATION_DELAY_CAPACITY || fabsf(fm_state.current_delay) > 1e-7f)
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
        if (phase_state.write_pos != frames % (int)APG_MODULATION_DELAY_CAPACITY)
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

    return test_modulation_delay_safety();
}
