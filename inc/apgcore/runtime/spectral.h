#ifndef AUDIO_PLAYGROUND_APGCORE_RUNTIME_SPECTRAL_H
#define AUDIO_PLAYGROUND_APGCORE_RUNTIME_SPECTRAL_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t fft_size;
    uint32_t bin_count;
    uint32_t hop_size;
} apg_spectral_info_t;

static inline bool apg_spectral_fft_size_supported(uint32_t fft_size) {
    return fft_size == 256u || fft_size == 512u || fft_size == 1024u || fft_size == 2048u;
}

static inline bool apg_spectral_info_valid(const apg_spectral_info_t *info) {
    return info && apg_spectral_fft_size_supported(info->fft_size) && info->bin_count == info->fft_size / 2u + 1u &&
           info->hop_size > 0u && info->hop_size <= info->fft_size;
}

#endif // AUDIO_PLAYGROUND_APGCORE_RUNTIME_SPECTRAL_H
