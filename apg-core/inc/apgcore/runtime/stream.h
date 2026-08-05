#ifndef AUDIO_PLAYGROUND_APGCORE_STREAM_H
#define AUDIO_PLAYGROUND_APGCORE_STREAM_H

#include <float.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t input_frames;
    uint32_t output_capacity;
    float    sample_rate;
    uint64_t sample_position;
} apg_stream_context_t;

typedef struct {
    uint32_t consumed_frames;
    uint32_t produced_frames;
} apg_stream_result_t;

static inline bool apg_stream_context_valid(const apg_stream_context_t *context) {
    return context && context->input_frames > 0u && context->output_capacity > 0u && context->sample_rate > 1.0f &&
           context->sample_rate <= FLT_MAX;
}

static inline apg_stream_result_t apg_stream_result_empty(void) {
    const apg_stream_result_t result = {0u, 0u};
    return result;
}

#endif // AUDIO_PLAYGROUND_APGCORE_STREAM_H
