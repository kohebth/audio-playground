#include "test_atom_basic_common.h"

int test_process_info_freq_shift_frame_limits(void) {
    const int frame_sizes[] = {64, 128, 256, 512, 1024};

    for (size_t c = 0; c < sizeof(frame_sizes) / sizeof(frame_sizes[0]); c++) {
        const int frames = frame_sizes[c];
        float     signal[1024];
        float     output[1024];
        float     buffer[8192];

        for (int i = 0; i < 1024; i++) {
            signal[i] = 0.0f;
            output[i] = -99.0f;
        }
        memset(buffer, 0, sizeof(buffer));

        apg_process_info_t info = {.sample_rate = 48000.0f, .frames = (uint32_t)frames, .channels = 1};

        freq_shift_out_t    out    = {.signal = output};
        freq_shift_in_t     in     = {.signal = signal, .pitch_shift = NULL};
        freq_shift_params_t params = {.block_size = frames};
        freq_shift_state_t  state  = {.window = NULL, .real = buffer, .imag = NULL, .write_pos = 0, .read_ptr = 128.0f};
        freq_shift_process(&out, &in, &params, &state, &info);

        for (int i = 0; i < frames; i++) {
            if (fabsf(output[i]) > 1e-7f)
                return fail("freq_shift_process mismatch");
        }
        if (frames < 1024 && output[frames] != -99.0f)
            return fail("freq_shift_process wrote past info.frames");
        if (state.write_pos != frames % 8192)
            return fail("freq_shift_process write_pos mismatch");
        if (fabsf(state.read_ptr - 128.0f) > 1e-7f)
            return fail("freq_shift_process read_ptr mismatch");
    }

    return 0;
}
