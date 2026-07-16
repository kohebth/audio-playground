#ifndef AUDIO_PLAYGROUND_APGCORE_PROCESS_H
#define AUDIO_PLAYGROUND_APGCORE_PROCESS_H

#include <float.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t frames;
    float    sample_rate;
    uint64_t sample_position;
} apg_process_context_t;

static inline bool apg_process_context_valid(const apg_process_context_t *context) {
    return context && context->frames > 0u && context->sample_rate > 1.0f && context->sample_rate <= FLT_MAX;
}

static inline uint32_t apg_process_context_frames(const apg_process_context_t *context) {
    return apg_process_context_valid(context) ? context->frames : 0u;
}

static inline float apg_process_context_sample_rate(const apg_process_context_t *context) {
    return apg_process_context_valid(context) ? context->sample_rate : 0.0f;
}

#endif // AUDIO_PLAYGROUND_APGCORE_PROCESS_H
