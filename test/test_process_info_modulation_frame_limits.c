#include "test_atom_basic_common.h"

static int test_modulation_phaser_response(void) {
    enum { FRAMES = 96 };
    float input[FRAMES];
    float low_modulator[FRAMES];
    float high_modulator[FRAMES];
    float low_output[FRAMES];
    float high_output[FRAMES];
    float repeat_output[FRAMES];
    float low_history[12]    = {0};
    float high_history[12]   = {0};
    float repeat_history[12] = {0};

    for (size_t i = 0u; i < FRAMES; i++) {
        input[i]          = i == 0u ? 1.0f : 0.0f;
        low_modulator[i]  = -1.0f;
        high_modulator[i] = 1.0f;
    }

    const apg_process_context_t info   = {.sample_rate = 48000.0f, .frames = FRAMES};
    modulation_phaser_params_t  params = {
         .center_frequency = 900.0f,
         .depth            = 0.7f,
         .feedback         = 0.35f,
    };
    modulation_phaser_out_t   low_out   = {.signal = low_output};
    modulation_phaser_in_t    low_in    = {.signal = input, .modulator = low_modulator};
    modulation_phaser_state_t low_state = {.buffer = low_history, .buffer_len = 12u, .feedback_sample = 0.0f};
    modulation_phaser_process(&low_out, &low_in, &params, &low_state, &info);

    modulation_phaser_out_t   high_out   = {.signal = high_output};
    modulation_phaser_in_t    high_in    = {.signal = input, .modulator = high_modulator};
    modulation_phaser_state_t high_state = {.buffer = high_history, .buffer_len = 12u, .feedback_sample = 0.0f};
    modulation_phaser_process(&high_out, &high_in, &params, &high_state, &info);

    modulation_phaser_out_t   repeat_out   = {.signal = repeat_output};
    modulation_phaser_in_t    repeat_in    = {.signal = input, .modulator = low_modulator};
    modulation_phaser_state_t repeat_state = {.buffer = repeat_history, .buffer_len = 12u, .feedback_sample = 0.0f};
    modulation_phaser_process(&repeat_out, &repeat_in, &params, &repeat_state, &info);

    float response_difference = 0.0f;
    float tail_energy         = 0.0f;
    for (size_t i = 0u; i < FRAMES; i++) {
        if (!isfinite(low_output[i]) || !isfinite(high_output[i]))
            return fail("modulation_phaser_process produced non-finite output");
        response_difference += fabsf(low_output[i] - high_output[i]);
        if (i > 6u)
            tail_energy += fabsf(low_output[i]);
        if (low_output[i] != repeat_output[i])
            return fail("modulation_phaser_process reset state was not deterministic");
    }
    if (response_difference < 0.1f)
        return fail("modulation_phaser_process did not sweep its all-pass response");
    if (tail_energy < 0.001f)
        return fail("modulation_phaser_process feedback did not create a finite tail");
    return 0;
}

int test_process_info_modulation_frame_limits(void) {
    const int frame_sizes[] = {64, 128, 256, 512, 1024};

    for (size_t c = 0; c < sizeof(frame_sizes) / sizeof(frame_sizes[0]); c++) {
        const int frames = frame_sizes[c];
        float     dry[1024];
        float     wet[1024];
        float     modulator[1024];
        float     y[1024];

        for (int i = 0; i < 1024; i++) {
            dry[i]       = 0.25f;
            wet[i]       = 0.75f;
            modulator[i] = (i % 2 == 0) ? 0.5f : -0.5f;
            y[i]         = -99.0f;
        }

        apg_process_context_t info = {.sample_rate = 48000.0f, .frames = (uint32_t)frames};

        generation_dc_out_t    dc_out = {.signal = y};
        generation_dc_in_t     dc_in;
        generation_dc_params_t dc_params = {.value = 0.375f};
        generation_dc_state_t  dc_state;
        generation_dc_process(&dc_out, &dc_in, &dc_params, &dc_state, &info);
        for (int i = 0; i < frames; i++) {
            if (fabsf(y[i] - dc_params.value) > 1e-7f)
                return fail("generation_dc_process value mismatch");
        }
        if (frames < 1024 && y[frames] != -99.0f)
            return fail("generation_dc_process wrote past info.frames");

        for (int i = 0; i < 1024; i++)
            y[i] = -99.0f;
        mix_wet_dry_out_t    mix_out    = {.signal = y};
        mix_wet_dry_in_t     mix_in     = {.dry = dry, .wet = wet};
        mix_wet_dry_params_t mix_params = {.mix = 0.25f};
        mix_wet_dry_state_t  mix_state;
        mix_wet_dry_process(&mix_out, &mix_in, &mix_params, &mix_state, &info);
        for (int i = 0; i < frames; i++) {
            if (fabsf(y[i] - 0.375f) > 1e-7f)
                return fail("mix_wet_dry_process mix mismatch");
        }
        if (frames < 1024 && y[frames] != -99.0f)
            return fail("mix_wet_dry_process wrote past info.frames");

        for (int i = 0; i < 1024; i++)
            y[i] = -99.0f;
        generation_lfo_out_t    lfo_out = {.signal = y};
        generation_lfo_in_t     lfo_in;
        generation_lfo_params_t lfo_params = {
            .frequency    = 1.0f,
            .waveform     = WAVEFORM_SINE,
            .phase_offset = 0.0f,
        };
        generation_lfo_state_t lfo_state = {.phase = 0.0f};
        generation_lfo_process(&lfo_out, &lfo_in, &lfo_params, &lfo_state, &info);
        if (assert_finite_buffer(y, frames, "generation_lfo_process"))
            return 1;
        if (frames < 1024 && y[frames] != -99.0f)
            return fail("generation_lfo_process wrote past info.frames");

        for (int i = 0; i < 1024; i++)
            y[i] = -99.0f;
        modulation_amplitude_out_t    amp_out    = {.signal = y};
        modulation_amplitude_in_t     amp_in     = {.signal = dry, .modulator = modulator};
        modulation_amplitude_params_t amp_params = {.depth = 0.5f};
        modulation_amplitude_state_t  amp_state;
        modulation_amplitude_process(&amp_out, &amp_in, &amp_params, &amp_state, &info);
        for (int i = 0; i < frames; i++) {
            float expected = dry[i] * (1.0f + amp_params.depth * modulator[i]);
            if (fabsf(y[i] - expected) > 1e-7f)
                return fail("modulation_amplitude_process mismatch");
        }
        if (frames < 1024 && y[frames] != -99.0f)
            return fail("modulation_amplitude_process wrote past info.frames");

        float phaser_history[12] = {0};
        for (int i = 0; i < 1024; i++)
            y[i] = -99.0f;
        modulation_phaser_out_t    phaser_out    = {.signal = y};
        modulation_phaser_in_t     phaser_in     = {.signal = dry, .modulator = modulator};
        modulation_phaser_params_t phaser_params = {
            .center_frequency = 900.0f,
            .depth            = 0.7f,
            .feedback         = 0.2f,
        };
        modulation_phaser_state_t phaser_state = {.buffer = phaser_history, .buffer_len = 12u, .feedback_sample = 0.0f};
        modulation_phaser_process(&phaser_out, &phaser_in, &phaser_params, &phaser_state, &info);
        if (assert_finite_buffer(y, frames, "modulation_phaser_process"))
            return 1;
        if (frames < 1024 && y[frames] != -99.0f)
            return fail("modulation_phaser_process wrote past info.frames");
    }

    return test_modulation_phaser_response();
}
