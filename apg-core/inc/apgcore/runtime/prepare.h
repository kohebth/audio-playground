#ifndef AUDIO_PLAYGROUND_APGCORE_PREPARE_H
#define AUDIO_PLAYGROUND_APGCORE_PREPARE_H

#include <float.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t maximum_frames;
    float    sample_rate;
} apg_prepare_context_t;

static inline bool apg_prepare_context_valid(const apg_prepare_context_t *context) {
    return context && context->maximum_frames > 0u && context->sample_rate > 1.0f && context->sample_rate <= FLT_MAX;
}

#endif // AUDIO_PLAYGROUND_APGCORE_PREPARE_H
