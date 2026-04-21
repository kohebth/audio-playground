#ifndef SUSTAINER_H
#define SUSTAINER_H

#include <atom/dsp_atoms.h>
#include <stdint.h>

typedef struct {
    float gain;      // Max gain (dB)
    float threshold; // Threshold (dBFS)
    float attack;    // Seconds
    float release;   // Seconds
    uint32_t sample_rate;
} SustainerParams;

typedef struct {
    struct { float prev_envelope; } detect_env_state;
    struct { float prev_value;    } smooth_state;
    struct { float z1; float z2;  } biquad_state;
} SustainerState;

typedef struct { float *signal; } sustainer_out_t;
typedef struct { float *signal; } sustainer_in_t;

void sustainer_process(
    sustainer_out_t out,
    sustainer_in_t in,
    SustainerParams params,
    SustainerState *state
);

#endif //SUSTAINER_H
