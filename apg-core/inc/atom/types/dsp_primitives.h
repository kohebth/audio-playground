#ifndef AUDIO_PLAYGROUND_DSP_PRIMITIVES_H
#define AUDIO_PLAYGROUND_DSP_PRIMITIVES_H

#include <apgcore/runtime/buffer.h>

/* Legacy names now carry capacity instead of erasing it behind float *. */
typedef apg_buffer_t Signal;
typedef apg_buffer_t Spectrum;
typedef apg_buffer_t Buffer;

#endif // AUDIO_PLAYGROUND_DSP_PRIMITIVES_H
