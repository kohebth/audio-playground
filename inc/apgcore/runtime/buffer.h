#ifndef AUDIO_PLAYGROUND_APGCORE_BUFFER_H
#define AUDIO_PLAYGROUND_APGCORE_BUFFER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    float   *data;
    uint32_t capacity;
} apg_buffer_t;

typedef struct {
    const float *data;
    uint32_t     length;
} apg_const_buffer_t;

static inline apg_buffer_t apg_buffer_make(float *data, uint32_t capacity) {
    return (apg_buffer_t){
        .data     = data,
        .capacity = capacity,
    };
}

static inline apg_const_buffer_t apg_const_buffer_make(const float *data, uint32_t length) {
    return (apg_const_buffer_t){
        .data   = data,
        .length = length,
    };
}

static inline bool apg_buffer_has_capacity(apg_buffer_t buffer, uint32_t required) {
    return buffer.data != NULL && buffer.capacity >= required;
}

static inline bool apg_const_buffer_has_length(apg_const_buffer_t buffer, uint32_t required) {
    return buffer.data != NULL && buffer.length >= required;
}

#endif // AUDIO_PLAYGROUND_APGCORE_BUFFER_H
