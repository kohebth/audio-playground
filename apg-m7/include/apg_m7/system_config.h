#ifndef APG_M7_SYSTEM_CONFIG_H
#define APG_M7_SYSTEM_CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* System Clocks (STM32F729 @ 216 MHz) */
#define APG_M7_SYSCLK_FREQ_HZ           216000000U
#define APG_M7_HCLK_FREQ_HZ             216000000U
#define APG_M7_APB1_FREQ_HZ             54000000U
#define APG_M7_APB2_FREQ_HZ             108000000U

/* Flash Memory Wait-States for 216 MHz @ 3.3V */
#define APG_M7_FLASH_LATENCY_CYCLES     7U

/* Audio Hardware & DMA Configuration */
#define APG_M7_AUDIO_SAMPLE_RATE_HZ     48000U
#define APG_M7_AUDIO_BLOCK_FRAMES       64U
#define APG_M7_AUDIO_CHANNELS           2U
#define APG_M7_CACHE_LINE_BYTES         32U

/* SAI Audio PLL Clock Configuration for Exact 48kHz (PLLI2SN=192, PLLI2SQ=2, PLLI2SR=2, PLLI2SDIVQ=1) */
#define APG_M7_PLLI2S_N                 192U
#define APG_M7_PLLI2S_Q                 2U
#define APG_M7_PLLI2S_R                 2U

/* Audio Hardware IO Selection:
 *   0 = APG_M7_AUDIO_IO_WM8960 (I2S / SAI + I2C Codec)
 *   1 = APG_M7_AUDIO_IO_GPIO_ADC_DAC (Native TIM-triggered GPIO/ADC/DAC)
 */
#define APG_M7_SELECTED_AUDIO_IO        0U

/* SAI1 Pin Mapping Definitions */
#define APG_M7_SAI1_MCLK_PIN            "PE2"
#define APG_M7_SAI1_FS_PIN              "PE4"
#define APG_M7_SAI1_SCK_PIN             "PE5"
#define APG_M7_SAI1_SD_A_PIN            "PE6"
#define APG_M7_WM8960_I2C_SCL_PIN       "PB8"
#define APG_M7_WM8960_I2C_SDA_PIN       "PB9"

/* Memory Budgeting (STM32F729 Memory Map) */
#define APG_M7_DTCM_START               0x20000000U
#define APG_M7_DTCM_SIZE_BYTES          (64U * 1024U)

#define APG_M7_SRAM1_START              0x20020000U
#define APG_M7_SRAM1_SIZE_BYTES         (368U * 1024U)

#define APG_M7_SRAM2_START              0x2007C000U
#define APG_M7_SRAM2_SIZE_BYTES         (16U * 1024U)

/* External SDRAM / DRAM Configuration (STM32 FMC Bank 5 / SDRAM Bank 1 - 8MB) */
#define APG_M7_SDRAM_START              0xC0000000U
#define APG_M7_SDRAM_SIZE_BYTES         (8U * 1024U * 1024U) /* 8 MB External DRAM */
#define APG_M7_FMC_SDRAM_CAS_LATENCY    2U
#define APG_M7_FMC_SDRAM_REFRESH_RATE   1386U
#define APG_M7_FMC_SDRAM_BUS_BITS       16U

/* USB Dual OTG Controller Allocation */
#define APG_M7_USB_OTG_HS_ROLE          "HOST_MSC"   /* USB OTG HS for USB Flash Drive (MSC / FatFS) */
#define APG_M7_USB_OTG_FS_ROLE          "DEVICE_DFU" /* USB OTG FS for WebUSB DFU Flashing */

/* FatFS & USB Preset Hot-Reload Configuration */
#define APG_M7_USB_MOUNT_POINT          "0:"
#define APG_M7_PRESET_SEARCH_PATH       "0:/presets"
#define APG_M7_ACTIVE_PRESET_FILE       "0:/presets/active.project.v2.yaml"
#define APG_M7_MAX_PRESET_SIZE_BYTES   (32U * 1024U)

typedef struct {
    uint32_t sys_clk_hz;
    uint32_t audio_sample_rate;
    uint16_t audio_block_frames;
    uint8_t audio_channels;
    bool usb_host_enabled;
    bool fs_mounted;
    char active_preset_path[128];
} apg_m7_system_status_t;

#ifdef __cplusplus
}
#endif

#endif /* APG_M7_SYSTEM_CONFIG_H */
