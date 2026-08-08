#ifndef APG_M7_AUDIO_ENGINE_H
#define APG_M7_AUDIO_ENGINE_H

#include "apg_m7/system_config.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APG_M7_ENGINE_UNINITIALIZED = 0,
    APG_M7_ENGINE_READY,
    APG_M7_ENGINE_RUNNING,
    APG_M7_ENGINE_ERROR
} apg_m7_engine_state_t;

typedef struct {
    uint32_t processed_blocks;
    uint32_t overrun_count;
    uint32_t underrun_count;
    float peak_cpu_load_percent;
} apg_m7_engine_stats_t;

#include "apg_m7/domain/audio_driver.h"

void apg_m7_audio_engine_init(void);
void apg_m7_audio_engine_init_with_driver(const apg_m7_audio_driver_t *driver);
bool apg_m7_audio_engine_load_yaml_preset(const char *yaml_content, size_t yaml_len);
void apg_m7_audio_engine_start(void);
void apg_m7_audio_engine_stop(void);

/* Audio callback function invoked from DMA Half-Transfer / Complete Transfer interrupt */
void apg_m7_audio_dma_callback(const float *input_stereo, float *output_stereo, uint32_t frames);

apg_m7_engine_state_t apg_m7_audio_engine_get_state(void);
void apg_m7_audio_engine_get_stats(apg_m7_engine_stats_t *out_stats);

#ifdef __cplusplus
}
#endif

#endif /* APG_M7_AUDIO_ENGINE_H */
