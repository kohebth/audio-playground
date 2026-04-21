#include <atom/dsp_atoms.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FFT_SIZE 4096

static void bit_reverse(float *data, uint32_t n) {
    uint32_t j = 0;
    for (uint32_t i = 0; i < n; i++) {
        if (i < j) {
            float temp_re = data[2 * i];
            float temp_im = data[2 * i + 1];
            data[2 * i] = data[2 * j];
            data[2 * i + 1] = data[2 * j + 1];
            data[2 * j] = temp_re;
            data[2 * j + 1] = temp_im;
        }
        uint32_t m = n >> 1;
        while (m >= 1 && j >= m) {
            j -= m;
            m >>= 1;
        }
        j += m;
    }
}

void freq_fft(freq_fft_out_t out, freq_fft_in_t in, freq_fft_params_t params, void *state) {
    if (out.real == NULL || out.imag == NULL || in.signal == NULL) return;

    int n = params.block_size;
    if (n > MAX_FFT_SIZE) n = MAX_FFT_SIZE;
    if ((n & (n - 1)) != 0) n = 1 << (32 - __builtin_clz(n)); 

    float temp[MAX_FFT_SIZE * 2];
    for (int i = 0; i < n; i++) {
        temp[2 * i] = (i < params.block_size) ? in.signal[i] : 0.0f;
        temp[2 * i + 1] = 0.0f;
    }

    bit_reverse(temp, n);

    for (int len = 2; len <= n; len <<= 1) {
        float ang = 2.0f * (float)M_PI / (float)len * -1.0f;
        float wlen_re = cosf(ang);
        float wlen_im = sinf(ang);
        for (int i = 0; i < n; i += len) {
            float w_re = 1.0f;
            float w_im = 0.0f;
            for (int j = 0; j < len / 2; j++) {
                float u_re = temp[2 * (i + j)];
                float u_im = temp[2 * (i + j) + 1];
                float v_re = temp[2 * (i + j + len / 2)] * w_re - temp[2 * (i + j + len / 2) + 1] * w_im;
                float v_im = temp[2 * (i + j + len / 2)] * w_im + temp[2 * (i + j + len / 2) + 1] * w_re;
                temp[2 * (i + j)] = u_re + v_re;
                temp[2 * (i + j) + 1] = u_im + v_im;
                temp[2 * (i + j + len / 2)] = u_re - v_re;
                temp[2 * (i + j + len / 2) + 1] = u_im - v_im;
                float tmp_re = w_re * wlen_re - w_im * wlen_im;
                w_im = w_re * wlen_im + w_im * wlen_re;
                w_re = tmp_re;
            }
        }
    }

    for (int i = 0; i < n; i++) {
        out.real[i] = temp[2 * i];
        out.imag[i] = temp[2 * i + 1];
    }
}
