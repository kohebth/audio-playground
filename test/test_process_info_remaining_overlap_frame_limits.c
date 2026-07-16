#include "test_atom_basic_common.h"

int test_process_info_remaining_overlap_frame_limits(void) {
    const int frame_sizes[] = {64, 128, 256, 512, 1024};

    for (size_t c = 0; c < sizeof(frame_sizes) / sizeof(frame_sizes[0]); c++) {
        const int frames = frame_sizes[c];
        float     signal[1024];
        float     frame[1024];
        float     output[1024];
        float     buffer[8192];

        for (int i = 0; i < 1024; i++) {
            signal[i] = (float)i;
            frame[i]  = (float)(i + 1);
            output[i] = -99.0f;
        }
        memset(buffer, 0, sizeof(buffer));

        apg_process_context_t info = {.sample_rate = 48000.0f, .frames = (uint32_t)frames};

        freq_overlap_add_out_t    add_out    = {.signal = output};
        freq_overlap_add_in_t     add_in     = {.frame = frame};
        freq_overlap_add_params_t add_params = {.block_size = frames, .hop_size = frames};
        freq_overlap_add_state_t  add_state  = {.buffer = buffer};
        freq_overlap_add_process(&add_out, &add_in, &add_params, &add_state, &info);

        for (int i = 0; i < frames; i++) {
            if (fabsf(output[i] - frame[i]) > 1e-7f)
                return fail("freq_overlap_add_process mismatch");
        }
        if (frames < 1024 && output[frames] != -99.0f)
            return fail("freq_overlap_add_process wrote past info.frames");
        for (int i = 0; i < frames; i++) {
            if (fabsf(buffer[i]) > 1e-7f)
                return fail("freq_overlap_add_process buffer mismatch");
        }

        for (int i = 0; i < 1024; i++)
            output[i] = -99.0f;
        memset(buffer, 0, sizeof(buffer));

        freq_overlap_save_out_t    save_out    = {.frame = output};
        freq_overlap_save_in_t     save_in     = {.signal = signal};
        freq_overlap_save_params_t save_params = {.block_size = frames, .hop_size = frames};
        freq_overlap_save_state_t  save_state  = {.buffer = buffer, .write_pos = 0};
        freq_overlap_save_process(&save_out, &save_in, &save_params, &save_state, &info);

        for (int i = 0; i < frames; i++) {
            if (fabsf(output[i] - signal[i]) > 1e-7f)
                return fail("freq_overlap_save_process mismatch");
        }
        if (frames < 1024 && output[frames] != -99.0f)
            return fail("freq_overlap_save_process wrote past info.frames");
        if (save_state.write_pos != frames % 1024)
            return fail("freq_overlap_save_process write_pos mismatch");
    }

    enum { FFT_SIZE = 256, HOP_SIZE = 64 };
    apg_spectral_info_t spectral = {.fft_size = FFT_SIZE, .bin_count = FFT_SIZE / 2 + 1, .hop_size = HOP_SIZE};
    float               hop[HOP_SIZE];
    float               frame[FFT_SIZE + 1];
    float               output[HOP_SIZE + 1];
    float               buffer[FFT_SIZE];
    for (int i = 0; i < HOP_SIZE; i++)
        hop[i] = (float)(i + 1);
    memset(buffer, 0, sizeof(buffer));
    frame[FFT_SIZE] = -99.0f;

    freq_overlap_save_out_t    save_out    = {.frame = frame};
    freq_overlap_save_in_t     save_in     = {.signal = hop};
    freq_overlap_save_params_t save_params = {.block_size = 512, .hop_size = 512};
    freq_overlap_save_state_t  save_state  = {.buffer = buffer, .write_pos = 37};
    freq_overlap_save_spectral_process(&save_out, &save_in, &save_params, &save_state, &spectral);
    for (int i = 0; i < FFT_SIZE - HOP_SIZE; i++) {
        if (frame[i] != 0.0f)
            return fail("freq_overlap_save spectral warm-up mismatch");
    }
    for (int i = 0; i < HOP_SIZE; i++) {
        if (frame[FFT_SIZE - HOP_SIZE + i] != hop[i])
            return fail("freq_overlap_save spectral hop mismatch");
    }
    if (frame[FFT_SIZE] != -99.0f || save_state.write_pos != 0)
        return fail("freq_overlap_save spectral extent mismatch");

    for (int i = 0; i < FFT_SIZE; i++)
        frame[i] = 1.0f;
    frame[FFT_SIZE] = output[HOP_SIZE] = -99.0f;
    memset(buffer, 0, sizeof(buffer));
    freq_overlap_add_out_t    add_out    = {.signal = output};
    freq_overlap_add_in_t     add_in     = {.frame = frame};
    freq_overlap_add_params_t add_params = {.block_size = 512, .hop_size = 512};
    freq_overlap_add_state_t  add_state  = {.buffer = buffer};
    freq_overlap_add_spectral_process(&add_out, &add_in, &add_params, &add_state, &spectral);
    for (int i = 0; i < HOP_SIZE; i++) {
        if (output[i] != 1.0f)
            return fail("freq_overlap_add spectral output mismatch");
    }
    for (int i = 0; i < FFT_SIZE - HOP_SIZE; i++) {
        if (buffer[i] != 1.0f)
            return fail("freq_overlap_add spectral tail mismatch");
    }
    for (int i = FFT_SIZE - HOP_SIZE; i < FFT_SIZE; i++) {
        if (buffer[i] != 0.0f)
            return fail("freq_overlap_add spectral clear mismatch");
    }
    if (output[HOP_SIZE] != -99.0f)
        return fail("freq_overlap_add wrote beyond hop_size");

    return 0;
}
