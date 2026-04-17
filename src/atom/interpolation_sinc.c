#include <atom/dsp_atoms.h>
#include <math.h>

#define PI 3.14159265358979323846f

static float sinc(float x) {
    if (fabsf(x) < 1e-6f) return 1.0f;
    return sinf(PI * x) / (PI * x);
}

static float blackman_window(float n, float M) {
    return 0.42f - 0.5f * cosf(2.0f * PI * n / M) + 0.08f * cosf(4.0f * PI * n / M);
}

float interpolation_sinc(SincInterpParams params) {
    if (params.buffer == NULL) return 0.0f;
    
    float pos = params.position;
    uint32_t taps = params.num_taps;
    if (taps % 2 == 0) taps++; // Ensure odd number of taps for symmetry
    
    int32_t center_idx = (int32_t)floorf(pos);
    int32_t half_taps = (int32_t)(taps / 2);
    
    float acc = 0.0f;
    for (int32_t i = -half_taps; i <= half_taps; ++i) {
        int32_t idx = center_idx + i;
        float x = (float)idx - pos;
        float w = blackman_window((float)(i + half_taps), (float)(taps - 1));
        acc += params.buffer[idx] * sinc(x) * w;
    }
    
    return acc;
}
