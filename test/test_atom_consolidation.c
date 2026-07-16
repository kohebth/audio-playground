#include "test_atom_basic_common.h"

#define CONSOLIDATION_FRAMES 32u

static int buffers_equal(const float *a, const float *b, uint32_t frames, float tolerance) {
    for (uint32_t i = 0u; i < frames; i++) {
        if (fabsf(a[i] - b[i]) > tolerance)
            return 0;
    }
    return 1;
}

static int test_oscillator_alias(void) {
    const apg_process_context_t context = {
        .frames          = CONSOLIDATION_FRAMES,
        .sample_rate     = 48000.0f,
        .sample_position = 0u,
    };
    float                       oscillator_output[CONSOLIDATION_FRAMES];
    float                       lfo_output[CONSOLIDATION_FRAMES];
    generation_oscillator_out_t oscillator_out = {.signal = oscillator_output};
    generation_oscillator_in_t  oscillator_in  = {.frequency = NULL};
    generation_lfo_out_t        lfo_out        = {.signal = lfo_output};
    generation_lfo_in_t         lfo_in         = {0};

    for (int waveform = WAVEFORM_SINE; waveform <= WAVEFORM_TRIANGLE; waveform++) {
        generation_oscillator_params_t oscillator_params = {
            .frequency    = 137.0f,
            .waveform     = waveform,
            .phase_offset = 0.17f,
        };
        generation_lfo_params_t lfo_params = {
            .frequency    = oscillator_params.frequency,
            .waveform     = waveform,
            .phase_offset = oscillator_params.phase_offset,
        };
        generation_oscillator_state_t oscillator_state = {.phase = 0.31f};
        generation_lfo_state_t        lfo_state        = {.phase = oscillator_state.phase};

        generation_oscillator_process(&oscillator_out, &oscillator_in, &oscillator_params, &oscillator_state, &context);
        generation_lfo_process(&lfo_out, &lfo_in, &lfo_params, &lfo_state, &context);
        if (!buffers_equal(oscillator_output, lfo_output, CONSOLIDATION_FRAMES, 0.0f) ||
            oscillator_state.phase != lfo_state.phase)
            return fail("generation_lfo does not delegate to generation_oscillator behavior");
    }
    return 0;
}

static int test_difference_aliases(void) {
    const apg_process_context_t context = {
        .frames          = CONSOLIDATION_FRAMES,
        .sample_rate     = 48000.0f,
        .sample_position = 0u,
    };
    float input[CONSOLIDATION_FRAMES];
    float canonical_output[CONSOLIDATION_FRAMES];
    float detect_output[CONSOLIDATION_FRAMES];
    float filter_output[CONSOLIDATION_FRAMES];
    for (uint32_t i = 0u; i < CONSOLIDATION_FRAMES; i++)
        input[i] = (float)((int)(i % 7u) - 3) * 0.25f;

    math_difference_out_t         canonical_out    = {.signal = canonical_output};
    math_difference_in_t          canonical_in     = {.signal = input};
    math_difference_params_t      canonical_params = {0};
    math_difference_state_t       canonical_state  = {.prev_sample = 0.375f};
    detect_slope_out_t            detect_out       = {.slope = detect_output};
    detect_slope_in_t             detect_in        = {.signal = input};
    detect_slope_params_t         detect_params    = {0};
    detect_slope_state_t          detect_state     = {.prev_sample = canonical_state.prev_sample};
    filter_differentiate_out_t    filter_out       = {.signal = filter_output};
    filter_differentiate_in_t     filter_in        = {.signal = input};
    filter_differentiate_params_t filter_params    = {0};
    filter_differentiate_state_t  filter_state     = {.prev_sample = canonical_state.prev_sample};

    math_difference_process(&canonical_out, &canonical_in, &canonical_params, &canonical_state, &context);
    detect_slope_process(&detect_out, &detect_in, &detect_params, &detect_state, &context);
    filter_differentiate_process(&filter_out, &filter_in, &filter_params, &filter_state, &context);
    if (!buffers_equal(canonical_output, detect_output, CONSOLIDATION_FRAMES, 0.0f) ||
        !buffers_equal(canonical_output, filter_output, CONSOLIDATION_FRAMES, 0.0f) ||
        canonical_state.prev_sample != detect_state.prev_sample ||
        canonical_state.prev_sample != filter_state.prev_sample)
        return fail("difference compatibility atoms diverged from math_difference");
    return 0;
}

static int test_integrate_aliases(void) {
    const apg_process_context_t context = {
        .frames          = CONSOLIDATION_FRAMES,
        .sample_rate     = 48000.0f,
        .sample_position = 0u,
    };
    float input[CONSOLIDATION_FRAMES];
    float canonical_output[CONSOLIDATION_FRAMES];
    float compatibility_output[CONSOLIDATION_FRAMES];
    for (uint32_t i = 0u; i < CONSOLIDATION_FRAMES; i++)
        input[i] = i == 0u ? 1.0f : -0.01f;

    math_integrate_out_t          canonical_out     = {.signal = canonical_output};
    math_integrate_in_t           canonical_in      = {.signal = input};
    math_integrate_params_t       canonical_params  = {.leakage = 1.0f};
    math_integrate_state_t        canonical_state   = {.accumulator = 0.25f};
    amplitude_accumulate_out_t    accumulate_out    = {.signal = compatibility_output};
    amplitude_accumulate_in_t     accumulate_in     = {.signal = input};
    amplitude_accumulate_params_t accumulate_params = {0};
    amplitude_accumulate_state_t  accumulate_state  = {.accumulator = canonical_state.accumulator};

    math_integrate_process(&canonical_out, &canonical_in, &canonical_params, &canonical_state, &context);
    amplitude_accumulate_process(&accumulate_out, &accumulate_in, &accumulate_params, &accumulate_state, &context);
    if (!buffers_equal(canonical_output, compatibility_output, CONSOLIDATION_FRAMES, 0.0f) ||
        canonical_state.accumulator != accumulate_state.accumulator)
        return fail("amplitude_accumulate diverged from math_integrate leakage 1");

    canonical_params.leakage                = 0.999f;
    canonical_state.accumulator             = -0.4f;
    filter_integrate_out_t    filter_out    = {.signal = compatibility_output};
    filter_integrate_in_t     filter_in     = {.signal = input};
    filter_integrate_params_t filter_params = {0};
    filter_integrate_state_t  filter_state  = {.accumulator = canonical_state.accumulator};
    math_integrate_process(&canonical_out, &canonical_in, &canonical_params, &canonical_state, &context);
    filter_integrate_process(&filter_out, &filter_in, &filter_params, &filter_state, &context);
    if (!buffers_equal(canonical_output, compatibility_output, CONSOLIDATION_FRAMES, 0.0f) ||
        canonical_state.accumulator != filter_state.accumulator)
        return fail("filter_integrate diverged from math_integrate leakage 0.999");
    return 0;
}

static int test_crossfade_alias_and_curves(void) {
    const apg_process_context_t context = {
        .frames          = CONSOLIDATION_FRAMES,
        .sample_rate     = 48000.0f,
        .sample_position = 0u,
    };
    float dry[CONSOLIDATION_FRAMES];
    float wet[CONSOLIDATION_FRAMES];
    float crossfade_output[CONSOLIDATION_FRAMES];
    float wet_dry_output[CONSOLIDATION_FRAMES];
    for (uint32_t i = 0u; i < CONSOLIDATION_FRAMES; i++) {
        dry[i] = 1.0f - (float)i * 0.01f;
        wet[i] = (float)i * 0.02f;
    }

    mix_crossfade_out_t    crossfade_out    = {.signal = crossfade_output};
    mix_crossfade_in_t     crossfade_in     = {.signal_a = dry, .signal_b = wet};
    mix_crossfade_params_t crossfade_params = {.t = 0.25f, .curve = 0};
    mix_crossfade_state_t  crossfade_state  = {0};
    mix_wet_dry_out_t      wet_dry_out      = {.signal = wet_dry_output};
    mix_wet_dry_in_t       wet_dry_in       = {.dry = dry, .wet = wet};
    mix_wet_dry_params_t   wet_dry_params   = {.mix = crossfade_params.t};
    mix_wet_dry_state_t    wet_dry_state    = {0};

    mix_crossfade_process(&crossfade_out, &crossfade_in, &crossfade_params, &crossfade_state, &context);
    mix_wet_dry_process(&wet_dry_out, &wet_dry_in, &wet_dry_params, &wet_dry_state, &context);
    if (!buffers_equal(crossfade_output, wet_dry_output, CONSOLIDATION_FRAMES, 0.0f))
        return fail("mix_wet_dry diverged from linear mix_crossfade");

    crossfade_params.t     = 0.5f;
    crossfade_params.curve = 1;
    mix_crossfade_process(&crossfade_out, &crossfade_in, &crossfade_params, &crossfade_state, &context);
    const float equal_power = sqrtf(0.5f);
    for (uint32_t i = 0u; i < CONSOLIDATION_FRAMES; i++) {
        const float expected = equal_power * (dry[i] + wet[i]);
        if (fabsf(crossfade_output[i] - expected) > 1.0e-6f)
            return fail("mix_crossfade equal-power curve is wrong");
    }
    return 0;
}

int test_atom_consolidation(void) {
    if (test_oscillator_alias())
        return 1;
    if (test_difference_aliases())
        return 1;
    if (test_integrate_aliases())
        return 1;
    return test_crossfade_alias_and_curves();
}
