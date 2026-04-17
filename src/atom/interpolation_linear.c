#include <atom/dsp_atoms.h>

float interpolation_linear(float sample_a, float sample_b, float t) {
    return sample_a * (1.0f - t) + sample_b * t;
}
