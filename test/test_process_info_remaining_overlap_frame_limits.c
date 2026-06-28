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

        apg_process_info_t info = {.sample_rate = 48000.0f, .frames = (uint32_t)frames, .channels = 1};

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

    return 0;
}
