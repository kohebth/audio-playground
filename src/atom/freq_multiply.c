#include <atom/dsp_atoms.h>

#define MAX_SPECTRUM_SIZE 4096

void freq_multiply(freq_multiply_out_t out, freq_multiply_in_t in, freq_multiply_params_t params, void *state) {
    if (out.real == NULL || out.imag == NULL || in.real_a == NULL || 
        in.imag_a == NULL || in.real_b == NULL || in.imag_b == NULL) return;

    for (int i = 0; i < params.block_size; i++) {
        float a_re = in.real_a[i];
        float a_im = in.imag_a[i];
        float b_re = in.real_b[i];
        float b_im = in.imag_b[i];

        out.real[i] = a_re * b_re - a_im * b_im;
        out.imag[i] = a_re * b_im + a_im * b_re;
    }
}
