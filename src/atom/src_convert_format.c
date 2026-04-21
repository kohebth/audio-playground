#include <atom/dsp_atoms.h>

#define CHUNK_LENGTH 512

void src_convert_format(src_convert_format_out_t out, src_convert_format_in_t in, src_convert_format_params_t params, void *state) {
    if (out.signal == NULL || in.signal == NULL) return;

    // Signal is always float* in this library, so this is currently a copy
    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        out.signal[i] = in.signal[i];
    }
}
