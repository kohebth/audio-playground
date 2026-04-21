#ifndef CHORUS_H
#define CHORUS_H

#include <atom/dsp_atoms.h>
#include <stdint.h>

typedef struct {
    float rate;      // Hz
    float depth;     // 0.0 to 1.0
    uint32_t sample_rate;
} ChorusParams;

typedef struct {
    struct { float phase; } lfo_state;
    struct { float z1; float z2; } pre_shelf_state;
    struct { float *buffer; int write_pos; } mod_state;
    struct { float z1; float z2; } de_shelf_state;
} ChorusState;

typedef struct { float *signal; } chorus_out_t;
typedef struct { float *signal; } chorus_in_t;

void chorus_process(
    chorus_out_t out,
    chorus_in_t in,
    ChorusParams params,
    ChorusState *state
);

#endif //CHORUS_H
