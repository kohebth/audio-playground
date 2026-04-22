#include <atom/dsp_atoms.h>
#include <math.h>
#include <stddef.h>

#define CHUNK_LENGTH 512

void nonlinear_bitcrush(
    nonlinear_bitcrush_out_t    *out,
    nonlinear_bitcrush_in_t     *in,
    nonlinear_bitcrush_params_t *params,
    nonlinear_bitcrush_state_t  *state
) {
    if (out->signal == NULL || in->signal == NULL)
        return;

    float levels = powf(2.0f, params->bit_depth);

    for (int i = 0; i < CHUNK_LENGTH; i++) {
        out->signal[i] = roundf(in->signal[i] * levels) / levels;
    }
}
