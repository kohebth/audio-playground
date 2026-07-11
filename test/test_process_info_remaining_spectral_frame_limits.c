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

    enum { FFT_SIZE = 256, BIN_COUNT = FFT_SIZE / 2 + 1 };
    float input[FFT_SIZE];
    float real[BIN_COUNT + 1];
    float imag[BIN_COUNT + 1];
    float reconstructed[FFT_SIZE + 1];
    for (int i = 0; i < FFT_SIZE; i++)
        input[i] = 0.25f * sinf(2.0f * (float)M_PI * 7.0f * (float)i / (float)FFT_SIZE) + (i == 0 ? 0.5f : 0.0f);
    real[BIN_COUNT] = imag[BIN_COUNT] = reconstructed[FFT_SIZE] = -99.0f;

    apg_spectral_info_t spectral   = {.fft_size = FFT_SIZE, .bin_count = BIN_COUNT, .hop_size = FFT_SIZE};
    freq_fft_out_t      fft_out    = {.real = real, .imag = imag};
    freq_fft_in_t       fft_in     = {.signal = input};
    freq_fft_params_t   fft_params = {.block_size = 512};
    float               fft_workspace[FFT_SIZE * 2];
    freq_fft_state_t    fft_state = {.workspace = fft_workspace, .buffer_len = FFT_SIZE * 2};
    freq_fft_process(&fft_out, &fft_in, &fft_params, &fft_state, &spectral);
    if (real[BIN_COUNT] != -99.0f || imag[BIN_COUNT] != -99.0f)
        return fail("freq_fft_process wrote beyond half spectrum");
    real[0] = -99.0f;
    fft_state.buffer_len--;
    freq_fft_process(&fft_out, &fft_in, &fft_params, &fft_state, &spectral);
    if (real[0] != -99.0f)
        return fail("freq_fft_process accepted undersized workspace");
    fft_state.buffer_len++;
    freq_fft_process(&fft_out, &fft_in, &fft_params, &fft_state, &spectral);

    float spectral_window[FFT_SIZE + 1];
    spectral_window[FFT_SIZE]                   = -99.0f;
    freq_window_out_t    spectral_window_out    = {.signal = spectral_window};
    freq_window_in_t     spectral_window_in     = {.signal = input};
    freq_window_params_t spectral_window_params = {.block_size = 512, .window_type = WINDOW_HANN};
    freq_window_state_t  spectral_window_state;
    freq_window_spectral_process(
        &spectral_window_out, &spectral_window_in, &spectral_window_params, &spectral_window_state, &spectral
    );
    if (spectral_window[FFT_SIZE] != -99.0f || spectral_window[0] != 0.0f ||
        fabsf(spectral_window[FFT_SIZE - 1]) > 1e-6f)
        return fail("freq_window spectral extent mismatch");

    freq_ifft_out_t    ifft_out    = {.signal = reconstructed};
    freq_ifft_in_t     ifft_in     = {.real = real, .imag = imag};
    freq_ifft_params_t ifft_params = {.block_size = 512};
    float              ifft_workspace[FFT_SIZE * 2];
    freq_ifft_state_t  ifft_state = {.workspace = ifft_workspace, .buffer_len = FFT_SIZE * 2};
    freq_ifft_process(&ifft_out, &ifft_in, &ifft_params, &ifft_state, &spectral);
    for (int i = 0; i < FFT_SIZE; i++) {
        if (!isfinite(reconstructed[i]) || fabsf(reconstructed[i] - input[i]) > 2e-5f)
            return fail("FFT/IFFT half-spectrum round trip mismatch");
    }
    if (reconstructed[FFT_SIZE] != -99.0f)
        return fail("freq_ifft_process wrote beyond fft_size");

    float a_real[BIN_COUNT], a_imag[BIN_COUNT], b_real[BIN_COUNT], b_imag[BIN_COUNT];
    float product_real[BIN_COUNT + 1], product_imag[BIN_COUNT + 1];
    for (int i = 0; i < BIN_COUNT; i++) {
        a_real[i] = 2.0f;
        a_imag[i] = 3.0f;
        b_real[i] = 4.0f;
        b_imag[i] = 5.0f;
    }
    a_real[3]               = NAN;
    product_real[BIN_COUNT] = product_imag[BIN_COUNT] = -99.0f;
    freq_multiply_out_t    multiply_out               = {.real = product_real, .imag = product_imag};
    freq_multiply_in_t     multiply_in     = {.real_a = a_real, .imag_a = a_imag, .real_b = b_real, .imag_b = b_imag};
    freq_multiply_params_t multiply_params = {.block_size = FFT_SIZE};
    freq_multiply_state_t  multiply_state;
    freq_multiply_process(&multiply_out, &multiply_in, &multiply_params, &multiply_state, &spectral);
    if (fabsf(product_real[0] + 7.0f) > 1e-7f || fabsf(product_imag[0] - 22.0f) > 1e-7f ||
        fabsf(product_real[3] + 15.0f) > 1e-7f || fabsf(product_imag[3] - 12.0f) > 1e-7f ||
        product_real[BIN_COUNT] != -99.0f || product_imag[BIN_COUNT] != -99.0f)
        return fail("freq_multiply_process contract mismatch");

    return 0;
}
