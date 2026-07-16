#include "test_atom_basic_common.h"

int test_process_info_arithmetic_mix_frame_limits(void) {
    const int frame_sizes[] = {64, 128, 256, 512, 1024};

    for (size_t c = 0; c < sizeof(frame_sizes) / sizeof(frame_sizes[0]); c++) {
        const int frames = frame_sizes[c];
        float     a[1024];
        float     b[1024];
        float     denom[1024];
        float     y[1024];

        for (int i = 0; i < 1024; i++) {
            a[i]     = (float)i * 0.01f;
            b[i]     = 0.25f;
            denom[i] = (i % 3 == 0) ? 0.0f : 0.5f;
            y[i]     = -99.0f;
        }

        apg_process_context_t info = {.sample_rate = 48000.0f, .frames = (uint32_t)frames};

        amplitude_add_out_t    add_out = {.signal = y};
        amplitude_add_in_t     add_in  = {.signal_a = a, .signal_b = b};
        amplitude_add_params_t add_params;
        amplitude_add_state_t  add_state;
        amplitude_add_process(&add_out, &add_in, &add_params, &add_state, &info);
        for (int i = 0; i < frames; i++) {
            if (fabsf(y[i] - (a[i] + b[i])) > 1e-7f)
                return fail("amplitude_add_process mismatch");
        }
        if (frames < 1024 && y[frames] != -99.0f)
            return fail("amplitude_add_process wrote past info.frames");

        for (int i = 0; i < 1024; i++)
            y[i] = -99.0f;
        amplitude_subtract_out_t    sub_out = {.signal = y};
        amplitude_subtract_in_t     sub_in  = {.signal_a = a, .signal_b = b};
        amplitude_subtract_params_t sub_params;
        amplitude_subtract_state_t  sub_state;
        amplitude_subtract_process(&sub_out, &sub_in, &sub_params, &sub_state, &info);
        for (int i = 0; i < frames; i++) {
            if (fabsf(y[i] - (a[i] - b[i])) > 1e-7f)
                return fail("amplitude_subtract_process mismatch");
        }
        if (frames < 1024 && y[frames] != -99.0f)
            return fail("amplitude_subtract_process wrote past info.frames");

        for (int i = 0; i < 1024; i++)
            y[i] = -99.0f;
        amplitude_divide_out_t    div_out    = {.signal = y};
        amplitude_divide_in_t     div_in     = {.numerator = a, .denominator = denom};
        amplitude_divide_params_t div_params = {.epsilon = 0.001f};
        amplitude_divide_state_t  div_state;
        amplitude_divide_process(&div_out, &div_in, &div_params, &div_state, &info);
        for (int i = 0; i < frames; i++) {
            float expected = (fabsf(denom[i]) > div_params.epsilon) ? a[i] / denom[i] : 0.0f;
            if (fabsf(y[i] - expected) > 1e-7f)
                return fail("amplitude_divide_process mismatch");
        }
        if (frames < 1024 && y[frames] != -99.0f)
            return fail("amplitude_divide_process wrote past info.frames");

        for (int i = 0; i < 1024; i++)
            y[i] = -99.0f;
        mix_crossfade_out_t    cross_out    = {.signal = y};
        mix_crossfade_in_t     cross_in     = {.signal_a = a, .signal_b = b};
        mix_crossfade_params_t cross_params = {.t = 0.25f};
        mix_crossfade_state_t  cross_state;
        mix_crossfade_process(&cross_out, &cross_in, &cross_params, &cross_state, &info);
        for (int i = 0; i < frames; i++) {
            float expected = 0.75f * a[i] + 0.25f * b[i];
            if (fabsf(y[i] - expected) > 1e-7f)
                return fail("mix_crossfade_process mismatch");
        }
        if (frames < 1024 && y[frames] != -99.0f)
            return fail("mix_crossfade_process wrote past info.frames");
    }

    return 0;
}
