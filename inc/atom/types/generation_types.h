#ifndef AUDIO_PLAYGROUND_GENERATION_TYPES_H
#define AUDIO_PLAYGROUND_GENERATION_TYPES_H

#include <atom/types/dsp_type_macros.h>

// clang-format off
#define APG_GENERATION_DSP_TYPE_TABLE(X) \
    X(generation_dc, SIGNAL, EMPTY, { float value; }, {}) \
    X(generation_envelope, SIGNAL, GATE, { float attack; float decay; float sustain; float release; float sample_rate; }, { float current_level; int stage; }) \
    X(generation_impulse, SIGNAL, EMPTY, { float interval; float sample_rate; }, { int counter; }) \
    X(generation_lfo, SIGNAL, EMPTY, { float frequency; int waveform; float phase_offset; float sample_rate; }, { float phase; }) \
    X(generation_noise, SIGNAL, EMPTY, { float amplitude; int color; }, { uint32_t seed; float prev_value; }) \
    X(generation_oscillator, SIGNAL, FREQUENCY, { float frequency; int waveform; float phase_offset; float sample_rate; }, { float phase; })
// clang-format on

APG_GENERATION_DSP_TYPE_TABLE(APG_DECLARE_DSP_TYPES)

#endif // AUDIO_PLAYGROUND_GENERATION_TYPES_H
