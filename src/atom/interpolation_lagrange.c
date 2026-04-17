#include <atom/dsp_atoms.h>

float interpolation_lagrange(LagrangeInterpParams params) {
    if (params.samples == NULL) return 0.0f;
    
    uint32_t n = params.order;
    float t = params.t;
    float result = 0.0f;
    
    for (uint32_t i = 0; i <= n; ++i) {
        float l_i = 1.0f;
        for (uint32_t j = 0; j <= n; ++j) {
            if (i == j) continue;
            l_i *= (t - (float)j) / ((float)i - (float)j);
        }
        result += params.samples[i] * l_i;
    }
    
    return result;
}
