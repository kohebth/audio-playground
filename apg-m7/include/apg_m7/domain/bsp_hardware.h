#ifndef APG_M7_BSP_HARDWARE_H
#define APG_M7_BSP_HARDWARE_H

#include "apg_m7/system_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APG_M7_BSP_OK = 0,
    APG_M7_BSP_ERROR_CLOCK,
    APG_M7_BSP_ERROR_SAI,
    APG_M7_BSP_ERROR_DMA,
    APG_M7_BSP_ERROR_CACHE
} apg_m7_bsp_status_t;

apg_m7_bsp_status_t apg_m7_bsp_init_clocks(void);
apg_m7_bsp_status_t apg_m7_bsp_init_sdram(void);
apg_m7_bsp_status_t apg_m7_bsp_init_audio_sai(uint32_t sample_rate);
apg_m7_bsp_status_t apg_m7_bsp_init_dma_buffers(void);

void apg_m7_bsp_clean_dcache(const void *addr, uint32_t size);
void apg_m7_bsp_invalidate_dcache(const void *addr, uint32_t size);

#ifdef __cplusplus
}
#endif

#endif /* APG_M7_BSP_HARDWARE_H */
