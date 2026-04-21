#include <atom/dsp_atoms.h>

#define CHUNK_LENGTH 512

void amplitude_divide(amplitude_divide_out_t out, amplitude_divide_in_t in, amplitude_divide_params_t params, void *state) {
    if (out.signal == NULL || in.numerator == NULL || in.denominator == NULL) return;
    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        if (in.denominator[i] > params.epsilon || in.denominator[i] < -params.epsilon) {
            out.signal[i] = in.numerator[i] / in.denominator[i];
        } else {
            out.signal[i] = 0.0f;
        }
    }
}
