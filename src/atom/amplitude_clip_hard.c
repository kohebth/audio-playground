#include <atom/dsp_atoms.h>
#include <fast_math.h>

#define CHUNK_LENGTH 512

void amplitude_clip_hard(amplitude_clip_hard_out_t out, amplitude_clip_hard_in_t in, amplitude_clip_hard_params_t params, void *state) {
    if (out.signal == NULL || in.signal == NULL) return;

    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        float s = in.signal[i];
        if (s > params.threshold) s = params.threshold;
        else if (s < -params.threshold) s = -params.threshold;
        out.signal[i] = s;
    }
}
