#include <atom/dsp_atoms.h>

float interpolation_cubic(CubicInterpParams params) {
    // Catmull-Rom Interpolation
    float t = params.t;
    float t2 = t * t;
    float t3 = t2 * t;
    
    float v0 = params.sample_n1;
    float v1 = params.sample_a;
    float v2 = params.sample_b;
    float v3 = params.sample_c;
    
    float a = -0.5f * v0 + 1.5f * v1 - 1.5f * v2 + 0.5f * v3;
    float b = v0 - 2.5f * v1 + 2.0f * v2 - 0.5f * v3;
    float c = -0.5f * v0 + 0.5f * v2;
    float d = v1;
    
    return a * t3 + b * t2 + c * t + d;
}
