#include "test_atom_basic_common.h"

int test_process_info_remaining_mix_nonlinear_frame_limits(void) {
    const int frame_sizes[] = {64, 128, 256, 512, 1024};

    for (size_t c = 0; c < sizeof(frame_sizes) / sizeof(frame_sizes[0]); c++) {
        const int frames = frame_sizes[c];
        float     mono[1024];
        float     left[1024];
        float     right[1024];
        float     mid[1024];
        float     side[1024];
        float     y[1024];
        float     table[4] = {0.0f, 10.0f, 20.0f, 30.0f};

        for (int i = 0; i < 1024; i++) {
            mono[i]  = (float)i * 0.25f - 1.0f;
            left[i]  = 1.0f;
            right[i] = 0.0f;
            mid[i]   = -99.0f;
            side[i]  = -99.0f;
            y[i]     = -99.0f;
        }

        apg_process_info_t info = {.sample_rate = 48000.0f, .frames = (uint32_t)frames, .channels = 2};

        float  matrix_a[1024];
        float  matrix_b[1024];
        float  matrix_out_l[1024];
        float  matrix_out_r[1024];
        float *matrix_inputs[2]  = {matrix_a, matrix_b};
        float *matrix_outputs[2] = {matrix_out_l, matrix_out_r};
        float  row_l[2]          = {0.25f, 0.75f};
        float  row_r[2]          = {1.0f, -1.0f};
        float *coefficients[2]   = {row_l, row_r};

        for (int i = 0; i < 1024; i++) {
            matrix_a[i]     = (float)i;
            matrix_b[i]     = 100.0f - (float)i;
            matrix_out_l[i] = -99.0f;
            matrix_out_r[i] = -99.0f;
        }

        mix_matrix_out_t    matrix_out    = {.signals = matrix_outputs};
        mix_matrix_in_t     matrix_in     = {.signals = matrix_inputs};
        mix_matrix_params_t matrix_params = {.coefficients = coefficients, .num_in = 2, .num_out = 2};
        mix_matrix_state_t  matrix_state;
        mix_matrix_process(&matrix_out, &matrix_in, &matrix_params, &matrix_state, &info);
        for (int i = 0; i < frames; i++) {
            float expected_l = matrix_a[i] * row_l[0] + matrix_b[i] * row_l[1];
            float expected_r = matrix_a[i] * row_r[0] + matrix_b[i] * row_r[1];
            if (fabsf(matrix_out_l[i] - expected_l) > 1e-7f || fabsf(matrix_out_r[i] - expected_r) > 1e-7f)
                return fail("mix_matrix_process mismatch");
        }
        if (frames < 1024 && (matrix_out_l[frames] != -99.0f || matrix_out_r[frames] != -99.0f))
            return fail("mix_matrix_process wrote past info.frames");

        mix_pan_stereo_out_t    pan_out    = {.left = left, .right = right};
        mix_pan_stereo_in_t     pan_in     = {.signal = mono};
        mix_pan_stereo_params_t pan_params = {.position = 0.25f};
        mix_pan_stereo_state_t  pan_state;
        mix_pan_stereo_process(&pan_out, &pan_in, &pan_params, &pan_state, &info);
        for (int i = 0; i < frames; i++) {
            float expected_left  = mono[i] * 0.75f;
            float expected_right = mono[i] * 0.25f;
            if (fabsf(left[i] - expected_left) > 1e-7f || fabsf(right[i] - expected_right) > 1e-7f)
                return fail("mix_pan_stereo_process mismatch");
        }
        if (frames < 1024 && (left[frames] != 1.0f || right[frames] != 0.0f))
            return fail("mix_pan_stereo_process wrote past info.frames");

        for (int i = 0; i < 1024; i++) {
            left[i]  = 1.0f;
            right[i] = 0.0f;
            y[i]     = -99.0f;
        }
        mix_encode_ms_out_t    enc_out = {.mid = mid, .side = side};
        mix_encode_ms_in_t     enc_in  = {.left = left, .right = right};
        mix_encode_ms_params_t enc_params;
        mix_encode_ms_state_t  enc_state;
        mix_encode_ms_process(&enc_out, &enc_in, &enc_params, &enc_state, &info);
        for (int i = 0; i < frames; i++) {
            if (fabsf(mid[i] - (float)M_SQRT1_2) > 1e-7f || fabsf(side[i] - (float)M_SQRT1_2) > 1e-7f)
                return fail("mix_encode_ms_process mismatch");
        }
        if (frames < 1024 && (mid[frames] != -99.0f || side[frames] != -99.0f))
            return fail("mix_encode_ms_process wrote past info.frames");

        for (int i = 0; i < 1024; i++) {
            mid[i]   = 1.0f;
            side[i]  = 0.0f;
            left[i]  = -99.0f;
            right[i] = -99.0f;
        }
        mix_decode_ms_out_t    dec_out = {.left = left, .right = right};
        mix_decode_ms_in_t     dec_in  = {.mid = mid, .side = side};
        mix_decode_ms_params_t dec_params;
        mix_decode_ms_state_t  dec_state;
        mix_decode_ms_process(&dec_out, &dec_in, &dec_params, &dec_state, &info);
        for (int i = 0; i < frames; i++) {
            if (fabsf(left[i] - (float)M_SQRT1_2) > 1e-7f || fabsf(right[i] - (float)M_SQRT1_2) > 1e-7f)
                return fail("mix_decode_ms_process mismatch");
        }
        if (frames < 1024 && (left[frames] != -99.0f || right[frames] != -99.0f))
            return fail("mix_decode_ms_process wrote past info.frames");

        for (int i = 0; i < 1024; i++) {
            mono[i] = (float)((i % 8) - 4) / 4.0f;
            y[i]    = -99.0f;
        }
        nonlinear_bitcrush_out_t    crush_out    = {.signal = y};
        nonlinear_bitcrush_in_t     crush_in     = {.signal = mono};
        nonlinear_bitcrush_params_t crush_params = {.bit_depth = 2.0f};
        nonlinear_bitcrush_state_t  crush_state;
        nonlinear_bitcrush_process(&crush_out, &crush_in, &crush_params, &crush_state, &info);
        for (int i = 0; i < frames; i++) {
            float expected = roundf(mono[i] * 4.0f) / 4.0f;
            if (fabsf(y[i] - expected) > 1e-7f)
                return fail("nonlinear_bitcrush_process mismatch");
        }
        if (frames < 1024 && y[frames] != -99.0f)
            return fail("nonlinear_bitcrush_process wrote past info.frames");

        for (int i = 0; i < 1024; i++)
            y[i] = -99.0f;
        nonlinear_waveshape_out_t    wave_out    = {.signal = y};
        nonlinear_waveshape_in_t     wave_in     = {.signal = mono};
        nonlinear_waveshape_params_t wave_params = {.transfer_table = table, .table_size = 4};
        nonlinear_waveshape_state_t  wave_state;
        nonlinear_waveshape_process(&wave_out, &wave_in, &wave_params, &wave_state, &info);
        for (int i = 0; i < frames; i++) {
            float pos = (mono[i] + 1.0f) * 1.5f;
            if (pos < 0.0f)
                pos = 0.0f;
            if (pos > 2.0f)
                pos = 2.0f;
            float expected = table[(int)floorf(pos)] * (1.0f - (pos - floorf(pos))) +
                             table[(int)floorf(pos) + 1] * (pos - floorf(pos));
            if (fabsf(y[i] - expected) > 1e-7f)
                return fail("nonlinear_waveshape_process mismatch");
        }
        if (frames < 1024 && y[frames] != -99.0f)
            return fail("nonlinear_waveshape_process wrote past info.frames");

        for (int i = 0; i < 1024; i++) {
            mono[i] = (float)i;
            y[i]    = -99.0f;
        }
        nonlinear_sample_hold_out_t    sr_out    = {.signal = y};
        nonlinear_sample_hold_in_t     sr_in     = {.signal = mono};
        nonlinear_sample_hold_params_t sr_params = {.factor = 2.0f};
        nonlinear_sample_hold_state_t  sr_state  = {.last_val = 0.0f, .counter = 0.0f};
        nonlinear_sample_hold_process(&sr_out, &sr_in, &sr_params, &sr_state, &info);
        for (int i = 0; i < frames; i++) {
            float expected = (i < 2) ? 0.0f : (float)((i / 2) * 2);
            if (fabsf(y[i] - expected) > 1e-7f)
                return fail("nonlinear_sample_hold_process mismatch");
        }
        if (frames < 1024 && y[frames] != -99.0f)
            return fail("nonlinear_sample_hold_process wrote past info.frames");
    }

    return 0;
}
