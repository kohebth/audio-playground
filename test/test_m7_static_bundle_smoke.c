#include <stdint.h>

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

int main(void) {
    apg_m7_project_init();

    float *signals = (float *)(void *)apg_m7_project_signal_buffers;
    for (size_t i = 0u; i < APG_M7_PROJECT_BLOCK_FRAMES; i++)
        signals[i] = 0.25f;

    apg_m7_project_process_block();

    float *output = &signals[APG_M7_PROJECT_BLOCK_FRAMES];
    for (size_t i = 0u; i < APG_M7_PROJECT_BLOCK_FRAMES; i++) {
        if (!near_float(output[i], 1.5f))
            return 1;
    }
    return 0;
}
