#include "test_atom_basic_common.h"

static int test_src_filter_safety(void) {
    float input[8] = {1.0f, NAN, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float output_a[9];
    float output_b[9];
    for (size_t i = 0; i < 9u; ++i) {
        output_a[i] = -99.0f;
        output_b[i] = -99.0f;
    }

    apg_process_context_t  info     = {.sample_rate = 96000.0f, .frames = 8u};
    src_antialias_out_t    out_a    = {.signal = output_a};
    src_antialias_out_t    out_b    = {.signal = output_b};
    src_antialias_in_t     in       = {.signal = input};
    src_antialias_params_t params_a = {.cutoff = 12000.0f, .sample_rate = 8000.0f};
    src_antialias_params_t params_b = {.cutoff = 12000.0f, .sample_rate = 192000.0f};
    src_antialias_state_t  state_a  = {.z1 = NAN, .z2 = INFINITY};
    src_antialias_state_t  state_b  = {.z1 = NAN, .z2 = INFINITY};

    src_antialias_process(&out_a, &in, &params_a, &state_a, &info);
    src_antialias_process(&out_b, &in, &params_b, &state_b, &info);
    if (assert_finite_buffer(output_a, 8, "src_antialias_process invalid input"))
        return 1;
    for (size_t i = 0; i < 8u; ++i) {
        if (fabsf(output_a[i] - output_b[i]) > 1e-7f)
            return fail("src_antialias_process used legacy params sample rate");
    }
    if (!isfinite(state_a.z1) || !isfinite(state_a.z2) || output_a[8] != -99.0f)
        return fail("src_antialias_process did not sanitize state or preserve sentinel");

    for (size_t i = 0; i < 9u; ++i)
        output_a[i] = -99.0f;
    src_antiimage_out_t    image_out    = {.signal = output_a};
    src_antiimage_in_t     image_in     = {.signal = input};
    src_antiimage_params_t image_params = {.cutoff = NAN, .sample_rate = 1.0f};
    src_antiimage_state_t  image_state  = {.z1 = -INFINITY, .z2 = NAN};
    src_antiimage_process(&image_out, &image_in, &image_params, &image_state, &info);
    if (assert_finite_buffer(output_a, 8, "src_antiimage_process invalid config"))
        return 1;
    if (!isfinite(image_state.z1) || !isfinite(image_state.z2) || output_a[8] != -99.0f)
        return fail("src_antiimage_process did not sanitize state or preserve sentinel");

    output_a[0] = -99.0f;
    info.frames = 0u;
    src_antiimage_process(&image_out, &image_in, &image_params, &image_state, &info);
    if (output_a[0] != -99.0f)
        return fail("src_antiimage_process wrote for zero frames");

    return 0;
}

int test_process_info_remaining_interpolation_src_frame_limits(void) {
    const int frame_sizes[] = {64, 128, 256, 512, 1024};

    for (size_t c = 0; c < sizeof(frame_sizes) / sizeof(frame_sizes[0]); c++) {
        const int frames = frame_sizes[c];
        float     a[1024];
        float     b[1024];
        float     t[1024];
        float     samples[2048];
        float     positions[1024];
        float     y[1024];
        float     src_in[1024];
        float     src_out[1024];

        for (int i = 0; i < 1024; i++) {
            a[i]         = 1.0f;
            b[i]         = 3.0f;
            t[i]         = 0.25f;
            samples[i]   = 2.0f;
            positions[i] = 16.5f;
            src_in[i]    = (float)i * 0.1f;
            src_out[i]   = -99.0f;
            y[i]         = -99.0f;
        }
        for (int i = 1024; i < 2048; i++)
            samples[i] = 2.0f;

        apg_process_context_t info = {.sample_rate = 48000.0f, .frames = (uint32_t)frames};

        interpolation_linear_out_t    lin_out = {.signal = y};
        interpolation_linear_in_t     lin_in  = {.signal_a = a, .signal_b = b, .t = t};
        interpolation_linear_params_t lin_params;
        interpolation_linear_state_t  lin_state;
        interpolation_linear_process(&lin_out, &lin_in, &lin_params, &lin_state, &info);
        for (int i = 0; i < frames; i++) {
            if (fabsf(y[i] - 1.5f) > 1e-7f)
                return fail("interpolation_linear_process mismatch");
        }
        if (frames < 1024 && y[frames] != -99.0f)
            return fail("interpolation_linear_process wrote past info.frames");

        for (int i = 0; i < 1024; i++)
            y[i] = -99.0f;
        interpolation_cubic_out_t    cub_out = {.signal = y};
        interpolation_cubic_in_t     cub_in  = {.signal_n1 = a, .signal_a = b, .signal_b = a, .signal_c = b, .t = t};
        interpolation_cubic_params_t cub_params;
        interpolation_cubic_state_t  cub_state;
        interpolation_cubic_process(&cub_out, &cub_in, &cub_params, &cub_state, &info);
        for (int i = 0; i < frames; i++) {
            float tt       = t[i];
            float tt2      = tt * tt;
            float tt3      = tt2 * tt;
            float v0       = a[i];
            float v1       = b[i];
            float v2       = a[i];
            float v3       = b[i];
            float ca       = -0.5f * v0 + 1.5f * v1 - 1.5f * v2 + 0.5f * v3;
            float cb       = v0 - 2.5f * v1 + 2.0f * v2 - 0.5f * v3;
            float cc       = -0.5f * v0 + 0.5f * v2;
            float cd       = v1;
            float expected = ca * tt3 + cb * tt2 + cc * tt + cd;
            if (fabsf(y[i] - expected) > 1e-7f)
                return fail("interpolation_cubic_process mismatch");
        }
        if (frames < 1024 && y[frames] != -99.0f)
            return fail("interpolation_cubic_process wrote past info.frames");

        for (int i = 0; i < 1024; i++)
            y[i] = -99.0f;
        interpolation_lagrange_out_t    lag_out    = {.signal = y};
        interpolation_lagrange_in_t     lag_in     = {.samples = samples, .t = positions};
        interpolation_lagrange_params_t lag_params = {.order = 2};
        interpolation_lagrange_state_t  lag_state;
        interpolation_lagrange_process(&lag_out, &lag_in, &lag_params, &lag_state, &info);
        for (int i = 0; i < frames; i++) {
            if (fabsf(y[i] - 2.0f) > 1e-7f)
                return fail("interpolation_lagrange_process mismatch");
        }
        if (frames < 1024 && y[frames] != -99.0f)
            return fail("interpolation_lagrange_process wrote past info.frames");

        for (int i = 0; i < 1024; i++)
            y[i] = -99.0f;
        interpolation_sinc_out_t    sinc_out    = {.signal = y};
        interpolation_sinc_in_t     sinc_in     = {.buffer = samples, .position = positions};
        interpolation_sinc_params_t sinc_params = {.num_taps = 5};
        interpolation_sinc_state_t  sinc_state;
        interpolation_sinc_process(&sinc_out, &sinc_in, &sinc_params, &sinc_state, &info);
        if (assert_finite_buffer(y, frames, "interpolation_sinc_process"))
            return 1;
        for (int i = 1; i < frames; i++) {
            if (fabsf(y[i] - y[0]) > 1e-7f)
                return fail("interpolation_sinc_process mismatch");
        }
        if (frames < 1024 && y[frames] != -99.0f)
            return fail("interpolation_sinc_process wrote past info.frames");

        for (int i = 0; i < 1024; i++) {
            src_in[i]  = (float)i * 0.1f;
            src_out[i] = -99.0f;
            y[i]       = -99.0f;
        }
        src_convert_format_out_t    fmt_out = {.signal = src_out};
        src_convert_format_in_t     fmt_in  = {.signal = src_in};
        src_convert_format_params_t fmt_params;
        src_convert_format_state_t  fmt_state;
        src_convert_format_process(&fmt_out, &fmt_in, &fmt_params, &fmt_state, &info);
        for (int i = 0; i < frames; i++) {
            if (fabsf(src_out[i] - src_in[i]) > 1e-7f)
                return fail("src_convert_format_process mismatch");
        }
        if (frames < 1024 && src_out[frames] != -99.0f)
            return fail("src_convert_format_process wrote past info.frames");

        for (int i = 0; i < 1024; i++)
            src_in[i] = 0.0f;
        src_antialias_out_t    aa_out    = {.signal = y};
        src_antialias_in_t     aa_in     = {.signal = src_in};
        src_antialias_params_t aa_params = {.cutoff = 12000.0f, .sample_rate = 48000.0f};
        src_antialias_state_t  aa_state  = {.z1 = 0.0f, .z2 = 0.0f};
        src_antialias_process(&aa_out, &aa_in, &aa_params, &aa_state, &info);
        for (int i = 0; i < frames; i++) {
            if (fabsf(y[i]) > 1e-7f)
                return fail("src_antialias_process mismatch");
        }
        if (fabsf(aa_state.z1) > 1e-7f || fabsf(aa_state.z2) > 1e-7f)
            return fail("src_antialias_process state mismatch");
        if (frames < 1024 && y[frames] != -99.0f)
            return fail("src_antialias_process wrote past info.frames");

        for (int i = 0; i < 1024; i++)
            y[i] = -99.0f;
        src_antiimage_out_t    ai_out    = {.signal = y};
        src_antiimage_in_t     ai_in     = {.signal = src_in};
        src_antiimage_params_t ai_params = {.cutoff = 12000.0f, .sample_rate = 48000.0f};
        src_antiimage_state_t  ai_state  = {.z1 = 0.0f, .z2 = 0.0f};
        src_antiimage_process(&ai_out, &ai_in, &ai_params, &ai_state, &info);
        for (int i = 0; i < frames; i++) {
            if (fabsf(y[i]) > 1e-7f)
                return fail("src_antiimage_process mismatch");
        }
        if (fabsf(ai_state.z1) > 1e-7f || fabsf(ai_state.z2) > 1e-7f)
            return fail("src_antiimage_process state mismatch");
        if (frames < 1024 && y[frames] != -99.0f)
            return fail("src_antiimage_process wrote past info.frames");
    }

    {
        const uint32_t frames = 16u;
        float          samples[16];
        float          positions[16];
        float          a[16];
        float          b[16];
        float          t[16];
        float          y[16];
        for (uint32_t i = 0; i < frames; ++i) {
            samples[i]   = (float)i;
            positions[i] = (i & 1u) ? 1000000.0f : -1000000.0f;
            a[i]         = 1.0f;
            b[i]         = 3.0f;
            t[i]         = i == 0u ? NAN : (i == 1u ? -4.0f : 4.0f);
            y[i]         = -99.0f;
        }
        apg_process_context_t info = {.sample_rate = 48000.0f, .frames = frames};

        interpolation_linear_out_t    linear_out = {.signal = y};
        interpolation_linear_in_t     linear_in  = {.signal_a = a, .signal_b = b, .t = t};
        interpolation_linear_params_t linear_params;
        interpolation_linear_state_t  linear_state;
        interpolation_linear_process(&linear_out, &linear_in, &linear_params, &linear_state, &info);
        if (assert_finite_buffer(y, frames, "interpolation_linear bounded controls"))
            return 1;

        interpolation_lagrange_out_t    lagrange_out    = {.signal = y};
        interpolation_lagrange_in_t     lagrange_in     = {.samples = samples, .t = positions};
        interpolation_lagrange_params_t lagrange_params = {.order = 1000};
        interpolation_lagrange_state_t  lagrange_state;
        interpolation_lagrange_process(&lagrange_out, &lagrange_in, &lagrange_params, &lagrange_state, &info);
        if (assert_finite_buffer(y, frames, "interpolation_lagrange bounded reads"))
            return 1;

        interpolation_sinc_out_t    sinc_out    = {.signal = y};
        interpolation_sinc_in_t     sinc_in     = {.buffer = samples, .position = positions};
        interpolation_sinc_params_t sinc_params = {.num_taps = 1000};
        interpolation_sinc_state_t  sinc_state;
        interpolation_sinc_process(&sinc_out, &sinc_in, &sinc_params, &sinc_state, &info);
        if (assert_finite_buffer(y, frames, "interpolation_sinc bounded reads"))
            return 1;
    }

    return test_src_filter_safety();
}
