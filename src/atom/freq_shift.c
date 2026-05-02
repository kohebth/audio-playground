#include <atom/dsp_atoms.h>
#include <stdlib.h>
#include <math.h>

#define CHUNK_LENGTH   512
#define MAX_DELAY      8192

void freq_shift(
    freq_shift_out_t    *out,
    freq_shift_in_t     *in,
    freq_shift_params_t *params,
    freq_shift_state_t  *state
) {
    if (out->signal == NULL || in->signal == NULL || state == NULL || state->real == NULL)
        return;

    float *buffer = state->real;
    int write_pos = state->write_pos;
    float read_ptr = state->read_ptr;

    // Reduced window size for lower latency (512 samples = ~10.7ms @ 48k)
    float window_size = 512.0f;
    float min_delay = 128.0f; // Minimum safety distance

    for (int i = 0; i < CHUNK_LENGTH; i++) {
        float ratio = (in->pitch_shift != NULL) ? in->pitch_shift[i] : 1.0f;
        
        if (ratio < 0.5f) ratio = 0.5f;
        if (ratio > 2.0f) ratio = 2.0f;

        buffer[write_pos] = in->signal[i];

        float phase = read_ptr / window_size;
        while (phase >= 1.0f) phase -= 1.0f;
        while (phase < 0.0f) phase += 1.0f;

        // Tap 1
        float tap1_pos = (float)write_pos - read_ptr;
        while (tap1_pos < 0) tap1_pos += MAX_DELAY;
        int i1_0 = (int)floorf(tap1_pos) % MAX_DELAY;
        int i1_1 = (i1_0 + 1) % MAX_DELAY;
        float frac1 = tap1_pos - floorf(tap1_pos);
        float s1 = buffer[i1_0] * (1.0f - frac1) + buffer[i1_1] * frac1;

        // Tap 2
        float tap2_ptr = read_ptr + (window_size * 0.5f);
        float tap2_pos = (float)write_pos - tap2_ptr;
        while (tap2_pos < 0) tap2_pos += MAX_DELAY;
        int i2_0 = (int)floorf(tap2_pos) % MAX_DELAY;
        int i2_1 = (i2_0 + 1) % MAX_DELAY;
        float frac2 = tap2_pos - floorf(tap2_pos);
        float s2 = buffer[i2_0] * (1.0f - frac2) + buffer[i2_1] * frac2;

        float weight = 0.5f - 0.5f * cosf(2.0f * (float)M_PI * phase);
        out->signal[i] = s1 * weight + s2 * (1.0f - weight);

        read_ptr += (1.0f - ratio);

        // Keep read_ptr within a safe but small range [min_delay, window_size * 2]
        // This keeps the total delay very low.
        if (read_ptr > (window_size * 2.0f)) read_ptr -= window_size;
        if (read_ptr < min_delay) read_ptr += window_size;

        write_pos = (write_pos + 1) % MAX_DELAY;
    }

    state->write_pos = write_pos;
    state->read_ptr = read_ptr;
}