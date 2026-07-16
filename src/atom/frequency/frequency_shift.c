#include <atom/dsp_atoms.h>
#include <math.h>
#include <stdlib.h>

#define MAX_DELAY 8192

void freq_shift_process(
    freq_shift_out_t          *out,
    const freq_shift_in_t     *in,
    const freq_shift_params_t *params,
    freq_shift_state_t        *state,
    const apg_process_info_t  *info
) {
    if (out == NULL || in == NULL || params == NULL || state == NULL)
        return;
    (void)params;
    if (!out || !in || !state || out->signal == NULL || in->signal == NULL || state->real == NULL)
        return;

    float *buffer    = state->real;
    int    write_pos = state->write_pos;
    float  read_ptr  = state->read_ptr;
    if (write_pos < 0 || write_pos >= MAX_DELAY)
        write_pos = 0;
    if (!isfinite(read_ptr) || read_ptr < 0.0f || read_ptr >= (float)MAX_DELAY)
        read_ptr = 128.0f;

    const float window_size = 512.0f;
    const float min_delay   = 128.0f;

    const uint32_t frames = apg_process_frames_or_default(info);
    for (uint32_t i = 0; i < frames; ++i) {
        float ratio = (in->pitch_shift != NULL) ? in->pitch_shift[i] : 1.0f;
        if (!isfinite(ratio))
            ratio = 1.0f;

        if (ratio < 0.5f)
            ratio = 0.5f;
        if (ratio > 2.0f)
            ratio = 2.0f;

        buffer[write_pos] = isfinite(in->signal[i]) ? in->signal[i] : 0.0f;

        float phase = read_ptr / window_size;
        while (phase >= 1.0f)
            phase -= 1.0f;
        while (phase < 0.0f)
            phase += 1.0f;

        float tap1_pos = (float)write_pos - read_ptr;
        while (tap1_pos < 0)
            tap1_pos += MAX_DELAY;
        int   i1_0  = (int)floorf(tap1_pos) % MAX_DELAY;
        int   i1_1  = (i1_0 + 1) % MAX_DELAY;
        float frac1 = tap1_pos - floorf(tap1_pos);
        float s1_0  = isfinite(buffer[i1_0]) ? buffer[i1_0] : 0.0f;
        float s1_1  = isfinite(buffer[i1_1]) ? buffer[i1_1] : 0.0f;
        float s1    = s1_0 * (1.0f - frac1) + s1_1 * frac1;

        float tap2_ptr = read_ptr + (window_size * 0.5f);
        float tap2_pos = (float)write_pos - tap2_ptr;
        while (tap2_pos < 0)
            tap2_pos += MAX_DELAY;
        int   i2_0  = (int)floorf(tap2_pos) % MAX_DELAY;
        int   i2_1  = (i2_0 + 1) % MAX_DELAY;
        float frac2 = tap2_pos - floorf(tap2_pos);
        float s2_0  = isfinite(buffer[i2_0]) ? buffer[i2_0] : 0.0f;
        float s2_1  = isfinite(buffer[i2_1]) ? buffer[i2_1] : 0.0f;
        float s2    = s2_0 * (1.0f - frac2) + s2_1 * frac2;

        float weight   = 0.5f - 0.5f * cosf(2.0f * (float)M_PI * phase);
        out->signal[i] = s1 * weight + s2 * (1.0f - weight);

        read_ptr += (1.0f - ratio);

        if (read_ptr > (window_size * 2.0f))
            read_ptr -= window_size;
        if (read_ptr < min_delay)
            read_ptr += window_size;

        write_pos = (write_pos + 1) % MAX_DELAY;
    }

    state->write_pos = write_pos;
    state->read_ptr  = read_ptr;
}
