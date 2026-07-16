#include "test_atom_basic_common.h"

int test_process_info_generator_frame_limits(void) {
    const int frame_sizes[] = {64, 128, 256, 512, 1024};

    for (size_t c = 0; c < sizeof(frame_sizes) / sizeof(frame_sizes[0]); c++) {
        const int frames = frame_sizes[c];
        float     freq[1024];
        float     gate[1024];
        float     y[1024];

        for (int i = 0; i < 1024; i++) {
            freq[i] = 12000.0f;
            gate[i] = 1.0f;
            y[i]    = -99.0f;
        }

        apg_process_context_t info = {.sample_rate = 48000.0f, .frames = (uint32_t)frames};

        generation_impulse_out_t    impulse_out = {.signal = y};
        generation_impulse_in_t     impulse_in;
        generation_impulse_params_t impulse_params = {.interval = 0.001f};
        generation_impulse_state_t  impulse_state  = {.counter = 0};
        generation_impulse_process(&impulse_out, &impulse_in, &impulse_params, &impulse_state, &info);
        for (int i = 0; i < frames; i++) {
            float expected = (i % 48 == 0) ? 1.0f : 0.0f;
            if (fabsf(y[i] - expected) > 1e-7f)
                return fail("generation_impulse_process mismatch");
        }
        if (impulse_state.counter != (48 - 1 - ((frames - 1) % 48)))
            return fail("generation_impulse_process state mismatch");
        if (frames < 1024 && y[frames] != -99.0f)
            return fail("generation_impulse_process wrote past info.frames");

        for (int i = 0; i < 1024; i++)
            y[i] = -99.0f;
        generation_noise_out_t    noise_out = {.signal = y};
        generation_noise_in_t     noise_in;
        generation_noise_params_t noise_params = {.amplitude = 0.25f, .color = WAVEFORM_NOISE_WHITE};
        generation_noise_state_t  noise_state  = {.seed = 7u, .prev_value = 0.0f};
        generation_noise_process(&noise_out, &noise_in, &noise_params, &noise_state, &info);
        if (assert_finite_buffer(y, frames, "generation_noise_process"))
            return 1;
        for (int i = 0; i < frames; i++) {
            if (fabsf(y[i]) > noise_params.amplitude + 1e-7f)
                return fail("generation_noise_process exceeded amplitude");
        }
        if (noise_state.seed != advance_lcg(7u, frames))
            return fail("generation_noise_process seed mismatch");
        if (frames < 1024 && y[frames] != -99.0f)
            return fail("generation_noise_process wrote past info.frames");

        for (int i = 0; i < 1024; i++)
            y[i] = -99.0f;
        generation_envelope_out_t    env_out    = {.signal = y};
        generation_envelope_in_t     env_in     = {.gate = gate};
        generation_envelope_params_t env_params = {
            .attack  = 0.001f,
            .decay   = 0.001f,
            .sustain = 0.5f,
            .release = 0.001f,
        };
        generation_envelope_state_t env_state = {.current_level = 0.0f, .stage = 0};
        generation_envelope_process(&env_out, &env_in, &env_params, &env_state, &info);
        if (assert_finite_buffer(y, frames, "generation_envelope_process"))
            return 1;
        if (y[0] <= 0.0f || y[frames - 1] < 0.49f || y[frames - 1] > 1.0f)
            return fail("generation_envelope_process level mismatch");
        if (env_state.stage < 2 || env_state.stage > 3)
            return fail("generation_envelope_process state mismatch");
        if (frames < 1024 && y[frames] != -99.0f)
            return fail("generation_envelope_process wrote past info.frames");

        for (int i = 0; i < 1024; i++)
            y[i] = -99.0f;
        generation_oscillator_out_t    osc_out    = {.signal = y};
        generation_oscillator_in_t     osc_in     = {.frequency = freq};
        generation_oscillator_params_t osc_params = {
            .frequency    = 12000.0f,
            .waveform     = WAVEFORM_SQUARE,
            .phase_offset = 0.0f,
        };
        generation_oscillator_state_t osc_state = {.phase = 0.0f};
        generation_oscillator_process(&osc_out, &osc_in, &osc_params, &osc_state, &info);
        for (int i = 0; i < frames; i++) {
            float expected = (i % 2) == 0 ? 0.0f : ((i % 4) == 1 ? 1.0f : -1.0f);
            if (fabsf(y[i] - expected) > 1e-7f)
                return fail("generation_oscillator_process mismatch");
        }
        float expected_phase = 0.25f * (float)(frames % 4);
        if (fabsf(osc_state.phase - expected_phase) > 1e-6f)
            return fail("generation_oscillator_process phase mismatch");
        if (frames < 1024 && y[frames] != -99.0f)
            return fail("generation_oscillator_process wrote past info.frames");
    }

    return 0;
}
