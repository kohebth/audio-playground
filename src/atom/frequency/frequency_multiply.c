#include <atom/dsp_atoms.h>
#include <math.h>
#include <stddef.h>

void freq_multiply_process(
    freq_multiply_out_t       *out,
    freq_multiply_in_t        *in,
    freq_multiply_params_t    *params,
    freq_multiply_state_t     *state,
    const apg_spectral_info_t *spectral_info
) {
    (void)params;
    (void)state;
    if (!out || !in || out->real == NULL || out->imag == NULL || in->real_a == NULL || in->imag_a == NULL ||
        in->real_b == NULL || in->imag_b == NULL || !apg_spectral_info_valid(spectral_info))
        return;

    for (uint32_t i = 0; i < spectral_info->bin_count; i++) {
        float a_re = isfinite(in->real_a[i]) ? in->real_a[i] : 0.0f;
        float a_im = isfinite(in->imag_a[i]) ? in->imag_a[i] : 0.0f;
        float b_re = isfinite(in->real_b[i]) ? in->real_b[i] : 0.0f;
        float b_im = isfinite(in->imag_b[i]) ? in->imag_b[i] : 0.0f;

        out->real[i] = a_re * b_re - a_im * b_im;
        out->imag[i] = a_re * b_im + a_im * b_re;
    }
}

void freq_multiply(
    freq_multiply_out_t *out, freq_multiply_in_t *in, freq_multiply_params_t *params, freq_multiply_state_t *state
) {
    apg_spectral_info_t info = {0};
    if (params && params->block_size > 0) {
        info.fft_size  = (uint32_t)params->block_size;
        info.bin_count = info.fft_size / 2u + 1u;
        info.hop_size  = info.fft_size;
    }
    freq_multiply_process(out, in, params, state, &info);
}
