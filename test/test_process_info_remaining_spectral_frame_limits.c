#include "test_atom_basic_common.h"

int test_process_info_remaining_spectral_frame_limits(void) {
    const int frame_sizes[] = {64, 128, 256, 512, 1024};

    for (size_t c = 0; c < sizeof(frame_sizes) / sizeof(frame_sizes[0]); c++) {
        const int frames = frame_sizes[c];
        float     signal[1024];
        float     windowed[1024];
        float     quantized[1024];

        for (int i = 0; i < 1024; i++) {
            signal[i]    = (i == 0) ? 0.0f : ((i % 2 == 0) ? 440.0f : 880.0f);
            windowed[i]  = -99.0f;
            quantized[i] = -99.0f;
        }

        apg_process_info_t info = {.sample_rate = 48000.0f, .frames = (uint32_t)frames, .channels = 1};

        freq_window_out_t    win_out    = {.signal = windowed};
        freq_window_in_t     win_in     = {.signal = signal};
        freq_window_params_t win_params = {.block_size = frames, .window_type = WINDOW_HANN};
        freq_window_state_t  win_state;
        freq_window_process(&win_out, &win_in, &win_params, &win_state, &info);
        for (int i = 0; i < frames; i++) {
            float factor   = (frames > 1) ? ((float)i / (float)(frames - 1)) : 0.0f;
            float expected = signal[i] * (0.5f * (1.0f - cosf(2.0f * (float)M_PI * factor)));
            if (fabsf(windowed[i] - expected) > 1e-7f)
                return fail("freq_window_process mismatch");
        }
        if (frames < 1024 && windowed[frames] != -99.0f)
            return fail("freq_window_process wrote past info.frames");

        for (int i = 0; i < 1024; i++)
            quantized[i] = -99.0f;
        freq_quantize_out_t    q_out = {.signal = quantized};
        freq_quantize_in_t     q_in  = {.signal = signal};
        freq_quantize_params_t q_params;
        freq_quantize_state_t  q_state;
        freq_quantize_process(&q_out, &q_in, &q_params, &q_state, &info);
        for (int i = 0; i < frames; i++) {
            float expected = signal[i];
            if (fabsf(quantized[i] - expected) > 1e-7f)
                return fail("freq_quantize_process mismatch");
        }
        if (frames < 1024 && quantized[frames] != -99.0f)
            return fail("freq_quantize_process wrote past info.frames");
    }

    return 0;
}
