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

Signal freq_ifft(Spectrum spectrum, uint32_t block_size) {
    if (spectrum == NULL) return NULL;
    
    static float signal_out[MAX_FFT_SIZE];
    static float work_buffer[MAX_FFT_SIZE * 2];
    
    uint32_t n = block_size;
    if (n > MAX_FFT_SIZE) n = MAX_FFT_SIZE;
    if ((n & (n - 1)) != 0) n = 1 << (32 - __builtin_clz(n)); // Power of 2
    
    // Copy spectrum to work buffer and conjugate for IFFT
    for (uint32_t i = 0; i < n; i++) {
        work_buffer[2 * i] = spectrum[2 * i];
        work_buffer[2 * i + 1] = -spectrum[2 * i + 1]; // Conjugate
    }
    
    bit_reverse(work_buffer, n);
    
    for (uint32_t len = 2; len <= n; len <<= 1) {
        float ang = 2.0f * M_PI / len * -1.0f;
        float wlen_re = cosf(ang);
        float wlen_im = sinf(ang);
        for (uint32_t i = 0; i < n; i += len) {
            float w_re = 1.0f;
            float w_im = 0.0f;
            for (uint32_t j = 0; j < len / 2; j++) {
                float u_re = work_buffer[2 * (i + j)];
                float u_im = work_buffer[2 * (i + j) + 1];
                float v_re = work_buffer[2 * (i + j + len / 2)] * w_re - work_buffer[2 * (i + j + len / 2) + 1] * w_im;
                float v_im = work_buffer[2 * (i + j + len / 2)] * w_im + work_buffer[2 * (i + j + len / 2) + 1] * w_re;
                work_buffer[2 * (i + j)] = u_re + v_re;
                work_buffer[2 * (i + j) + 1] = u_im + v_im;
                work_buffer[2 * (i + j + len / 2)] = u_re - v_re;
                work_buffer[2 * (i + j + len / 2) + 1] = u_im - v_im;
                float tmp_re = w_re * wlen_re - w_im * wlen_im;
                w_im = w_re * wlen_im + w_im * wlen_re;
                w_re = tmp_re;
            }
        }
    }
    
    // Normalize and conjugate back (or just take real part and normalize)
    for (uint32_t i = 0; i < block_size; i++) {
        signal_out[i] = work_buffer[2 * i] / (float)n;
    }
    
    return signal_out;
}
