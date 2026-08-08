#include "apg_m7/system_config.h"
#include "apg_m7/domain/bsp_hardware.h"
#include "apg_m7/domain/usb_host.h"
#include "apg_m7/domain/preset_fs.h"
#include "apg_m7/domain/audio_engine.h"
#include <stdio.h>

#include "apg_m7/domain/usb_dfu.h"

static char g_preset_buffer[APG_M7_MAX_PRESET_SIZE_BYTES];

int main(void) {
    /* 1. System Clocks & Hardware Peripherals */
    if (apg_m7_bsp_init_clocks() != APG_M7_BSP_OK) {
        return 1;
    }
    if (apg_m7_bsp_init_sdram() != APG_M7_BSP_OK) {
        return 1;
    }
    if (apg_m7_bsp_init_audio_sai(APG_M7_AUDIO_SAMPLE_RATE_HZ) != APG_M7_BSP_OK) {
        return 1;
    }
    if (apg_m7_bsp_init_dma_buffers() != APG_M7_BSP_OK) {
        return 1;
    }

    /* 2. Initialize Subsystems (Audio, USB Host MSC, USB Device DFU) */
    apg_m7_audio_engine_init();
    apg_m7_usb_init();
    apg_m7_usb_dfu_init();

    /* 3. Main Loop */
    while (1) {
        /* USB Host Task Processing */
        apg_m7_usb_process();

        /* USB Mass Storage Preset Mounting & Hot-Reload */
        if (apg_m7_usb_is_storage_ready()) {
            if (apg_m7_fs_mount() == APG_M7_FS_OK) {
                size_t bytes_read = 0;
                if (apg_m7_fs_load_preset_yaml(APG_M7_ACTIVE_PRESET_FILE,
                                               g_preset_buffer,
                                               sizeof(g_preset_buffer),
                                               &bytes_read) == APG_M7_FS_OK) {
                    
                    if (apg_m7_audio_engine_load_yaml_preset(g_preset_buffer, bytes_read)) {
                        apg_m7_audio_engine_start();
                    }
                }
            }
        }

#if defined(__ARM_ARCH_7EM__) || defined(__arm__)
        /* Sleep until next interrupt (DMA / USB / Timer) to reduce power & heat */
        __asm__ volatile ("wfi");
#endif
    }

    return 0;
}
