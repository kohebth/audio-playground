#ifndef APG_M7_AUDIO_DRIVER_H
#define APG_M7_AUDIO_DRIVER_H

#include "apg_m7/system_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APG_M7_AUDIO_IO_WM8960 = 0,     /* I2S / SAI + I2C Control (WM8960 Codec) */
    APG_M7_AUDIO_IO_GPIO_ADC_DAC = 1  /* Parallel/Timer-triggered GPIO/Internal ADC + DAC */
} apg_m7_audio_io_mode_t;

typedef enum {
    APG_M7_AUDIO_DRIVER_OK = 0,
    APG_M7_AUDIO_DRIVER_ERROR_INIT,
    APG_M7_AUDIO_DRIVER_ERROR_CONFIG,
    APG_M7_AUDIO_DRIVER_ERROR_I2C,
    APG_M7_AUDIO_DRIVER_ERROR_DMA
} apg_m7_audio_driver_status_t;

/* WM8960 Codec I2C Address (0x1A or 0x34 shifted) */
#define WM8960_I2C_ADDR                 0x1AU

/* Audio Driver Public Interface */
typedef struct {
    apg_m7_audio_io_mode_t mode;
    apg_m7_audio_driver_status_t (*init)(uint32_t sample_rate);
    apg_m7_audio_driver_status_t (*start)(void);
    apg_m7_audio_driver_status_t (*stop)(void);
    apg_m7_audio_driver_status_t (*set_volume)(uint8_t volume_percent);
} apg_m7_audio_driver_t;

/* Driver Instance Constructors */
const apg_m7_audio_driver_t *apg_m7_audio_driver_get_wm8960(void);
const apg_m7_audio_driver_t *apg_m7_audio_driver_get_gpio_adc_dac(void);

#ifdef __cplusplus
}
#endif

#endif /* APG_M7_AUDIO_DRIVER_H */
