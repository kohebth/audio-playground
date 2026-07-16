#ifndef AUDIO_PLAYGROUND_DSP_PORTS_H
#define AUDIO_PLAYGROUND_DSP_PORTS_H

typedef struct {
    float *signal;
} atom_mono_t;

typedef struct {
    float *signal_a;
    float *signal_b;
} atom_pair_t;

typedef struct {
    float *left;
    float *right;
} atom_stereo_t;

typedef struct {
    float *real;
    float *imag;
} atom_complex_t;

typedef struct {
    float *mid;
    float *side;
} atom_ms_t;

typedef struct {
    float *dry;
    float *wet;
} atom_wet_dry_t;

typedef struct {
    float *numerator;
    float *denominator;
} atom_div_t;

#endif // AUDIO_PLAYGROUND_DSP_PORTS_H
