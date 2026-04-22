#include <atom/dsp_atoms.h>
#include <math.h>
#include <stddef.h>

#define CHUNK_LENGTH 512

void freq_window(
    freq_window_out_t *out, freq_window_in_t *in, freq_window_params_t *params, freq_window_state_t *state
) {
    if (out->signal == NULL || in->signal == NULL)
        return;

    int N = params->block_size;
    if (N < 1)
        N = CHUNK_LENGTH;

    for (int i = 0; i < N; ++i) {
        float w      = 1.0f;
        float factor = (float)i / (float)(N - 1);

        switch (params->window_type) {
        case WINDOW_HANN:
            w = 0.5f * (1.0f - cosf(2.0f * (float)M_PI * factor));
            break;
        case WINDOW_HAMMING:
            w = 0.54f - 0.46f * cosf(2.0f * (float)M_PI * factor);
            break;
        case WINDOW_BLACKMAN:
            w = 0.42f - 0.5f * cosf(2.0f * (float)M_PI * factor) + 0.08f * cosf(4.0f * (float)M_PI * factor);
            break;
        default:
            w = 1.0f;
            break;
        }
        out->signal[i] = in->signal[i] * w;
    }
}
