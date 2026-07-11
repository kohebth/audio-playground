#include <atom/dsp_atoms.h>
#include <math.h>
#include <stddef.h>

void freq_window_spectral_process(
    freq_window_out_t         *out,
    freq_window_in_t          *in,
    freq_window_params_t      *params,
    freq_window_state_t       *state,
    const apg_spectral_info_t *spectral_info
) {
    (void)state;
    if (!out || !in || !params || !out->signal || !in->signal || !apg_spectral_info_valid(spectral_info))
        return;

    const uint32_t n = spectral_info->fft_size;
    for (uint32_t i = 0; i < n; i++) {
        const float factor = n > 1u ? (float)i / (float)(n - 1u) : 0.0f;
        float       w      = 1.0f;
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
            break;
        }
        const float sample = isfinite(in->signal[i]) ? in->signal[i] : 0.0f;
        out->signal[i]     = sample * w;
    }
}

void freq_window_process(
    freq_window_out_t        *out,
    freq_window_in_t         *in,
    freq_window_params_t     *params,
    freq_window_state_t      *state,
    const apg_process_info_t *info
) {
    if (out->signal == NULL || in->signal == NULL)
        return;

    int N = params->block_size;
    if (N < 1)
        N = (int)apg_process_frames_or_default(info);

    const uint32_t frames = apg_process_frames_or_default(info);
    if (N > (int)frames)
        N = (int)frames;

    for (int i = 0; i < N; ++i) {
        float w      = 1.0f;
        float factor = (N > 1) ? ((float)i / (float)(N - 1)) : 0.0f;

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

void freq_window(
    freq_window_out_t *out, freq_window_in_t *in, freq_window_params_t *params, freq_window_state_t *state
) {
    const apg_process_info_t info = apg_process_info_default();
    freq_window_process(out, in, params, state, &info);
}
