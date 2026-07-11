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

    float signal[2]            = {NAN, 0.0f};
    float pitch[2]             = {INFINITY, 1.0f};
    float output[3]            = {-99.0f, -99.0f, -99.0f};
    float buffer[8192]         = {0.0f};
    buffer[128]                = NAN;
    apg_process_info_t  info   = {.sample_rate = 48000.0f, .frames = 2u, .channels = 1u};
    freq_shift_out_t    out    = {.signal = output};
    freq_shift_in_t     in     = {.signal = signal, .pitch_shift = pitch};
    freq_shift_params_t params = {.block_size = 999};
    freq_shift_state_t  state  = {.real = buffer, .write_pos = -7, .read_ptr = NAN};
    freq_shift_process(&out, &in, &params, &state, &info);
    if (!isfinite(output[0]) || !isfinite(output[1]) || output[2] != -99.0f || state.write_pos != 2 ||
        !isfinite(state.read_ptr))
        return fail("freq_shift_process did not normalize invalid input/state");

    return 0;
}
