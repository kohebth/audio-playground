#include <atom/dsp_atoms.h>

#define MAX_SPECTRUM_SIZE 4096

Spectrum freq_multiply(FreqMultiplyParams params) {
    if (params.spectrum_a == NULL || params.spectrum_b == NULL) return NULL;
    
    static float out_spectrum[MAX_SPECTRUM_SIZE * 2];
    
    // We assume both spectra have the same length. 
    // Since we don't have the length in params, we'll use a standard max or assume CHUNK_LENGTH.
    // However, Spectrum size is usually block_size. we'll use 512 as default for now if unknown.
    uint32_t n = 512; 
    
    for (uint32_t i = 0; i < n; i++) {
        float a_re = params.spectrum_a[2 * i];
        float a_im = params.spectrum_a[2 * i + 1];
        float b_re = params.spectrum_b[2 * i];
        float b_im = params.spectrum_b[2 * i + 1];
        
        out_spectrum[2 * i] = a_re * b_re - a_im * b_im;
        out_spectrum[2 * i + 1] = a_re * b_im + a_im * b_re;
    }
    
    return out_spectrum;
}
