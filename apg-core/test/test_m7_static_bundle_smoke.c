#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include "apg_project_m7.h"

#if APG_M7_PROJECT_SIGNAL_BUFFER_BYTES > 0u
extern uint8_t apg_m7_project_signal_buffers[APG_M7_PROJECT_SIGNAL_BUFFER_BYTES];
#endif

static int near_float(float actual, float expected) {
    float diff = actual - expected;
    if (diff < 0.0f)
        diff = -diff;
    return diff < 0.0001f;
}

static void seed_input(void) {
    float *signals = (float *)(void *)apg_m7_project_signal_buffers;
    for (size_t i = 0u; i < APG_M7_PROJECT_BLOCK_FRAMES; i++)
        signals[i] = 0.25f;
}

int main(void) {
    apg_m7_project_init();

    float *signals = (float *)(void *)apg_m7_project_signal_buffers;
    seed_input();
    apg_m7_project_process_block();

    float *output = &signals[APG_M7_PROJECT_BLOCK_FRAMES];
    for (size_t i = 0u; i < APG_M7_PROJECT_BLOCK_FRAMES; i++) {
        if (!near_float(output[i], 1.5f))
            return 1;
    }

    enum { ITERATIONS = 20000 };
    clock_t start = clock();
    for (int i = 0; i < ITERATIONS; i++) {
        seed_input();
        apg_m7_project_process_block();
    }
    clock_t end = clock();
    if (start == (clock_t)-1 || end == (clock_t)-1 || end < start)
        return 2;

    double elapsed_us = ((double)(end - start) * 1000000.0) / (double)CLOCKS_PER_SEC;
    double block_us   = elapsed_us / (double)ITERATIONS;
    printf("m7_static_host_block_us=%.3f budget_us=1000.000 iterations=%d\n", block_us, ITERATIONS);
    if (block_us > 1000.0)
        return 3;
    return 0;
}
