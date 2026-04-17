#include <atom/dsp_atoms.h>
#include <stdlib.h>

#define CHUNK_LENGTH 512
#define MAX_AUTOCORR_LAG 1024

Signal detect_autocorrelate(Signal signal, AutocorrelateParams params) {
    if (signal == NULL) return NULL;
    
    static float out_buffer[CHUNK_LENGTH];
    uint32_t max_lag = params.max_lag;
    if (max_lag > CHUNK_LENGTH) max_lag = CHUNK_LENGTH;
    
    for (uint32_t k = 0; k < max_lag; ++k) {
        float r_k = 0.0f;
        for (uint32_t n = k; n < CHUNK_LENGTH; ++n) {
            r_k += signal[n] * signal[n - k];
        }
        out_buffer[k] = r_k;
    }
    
    // Zero out the rest of the buffer
    for (uint32_t k = max_lag; k < CHUNK_LENGTH; ++k) {
        out_buffer[k] = 0.0f;
    }
    
    return out_buffer;
}
