#ifndef TEST_ATOM_BASIC_COMMON_H
#define TEST_ATOM_BASIC_COMMON_H

#include <apgcore/process.h>
#include <atom/dsp_atoms.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

#define TEST_CHUNK 512

static inline int fail(const char *msg) {
    fprintf(stderr, "FAIL: %s\n", msg);
    return 1;
}

static inline int assert_finite_buffer(const float *x, int n, const char *label) {
    for (int i = 0; i < n; i++) {
        if (!isfinite(x[i])) {
            fprintf(stderr, "FAIL: %s produced non-finite sample at %d\n", label, i);
            return 1;
        }
    }
    return 0;
}

static inline uint32_t advance_lcg(uint32_t seed, int frames) {
    for (int i = 0; i < frames; i++)
        seed = seed * 1664525u + 1013904223u;
    return seed;
}

#endif
