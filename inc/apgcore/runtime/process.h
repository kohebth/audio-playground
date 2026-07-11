#ifndef AUDIO_PLAYGROUND_APGCORE_PROCESS_H
#define AUDIO_PLAYGROUND_APGCORE_PROCESS_H

#include <stdint.h>

#define APG_DEFAULT_FRAMES 512u

typedef struct {
    float    sample_rate;
    uint32_t frames;
    uint32_t output_frames;
    uint32_t channels;
} apg_process_info_t;

static inline apg_process_info_t apg_process_info_default(void) {
    apg_process_info_t info = {48000.0f, APG_DEFAULT_FRAMES, APG_DEFAULT_FRAMES, 1u};
    return info;
}

static inline uint32_t apg_process_frames_or_default(const apg_process_info_t *info) {
    return info ? info->frames : APG_DEFAULT_FRAMES;
}

static inline uint32_t apg_process_output_frames_or_default(const apg_process_info_t *info) {
    return info && info->output_frames > 0u ? info->output_frames : apg_process_frames_or_default(info);
}

#endif // AUDIO_PLAYGROUND_APGCORE_PROCESS_H
