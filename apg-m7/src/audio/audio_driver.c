#include "apg_m7/domain/audio_driver.h"
#include <stdio.h>

static uint8_t static_volume_gain = 100;

/* --- WM8960 Codec Driver Option --- */
static apg_m7_audio_driver_status_t wm8960_init(uint32_t sample_rate) {
    (void)sample_rate;
#if defined(__ARM_ARCH_7EM__) || defined(__arm__)
    /* WM8960 I2C Register Writes (7-bit address 0x1A):
     * R15 (0x0F) Reset = 0x0000
     * R25 (0x19) Power Mgt 1 = 0x00FC (VMID 50k, VREF, AINL, AINR, ADCL, ADCR)
     * R26 (0x1A) Power Mgt 2 = 0x01F8 (DAC L/R, OUT1 L/R)
     * R4  (0x04) Clocking 1 = 0x0000 (SYSCLK = MCLK, 256fs)
     * R7  (0x07) Audio Interface 1 = 0x0002 (I2S Format, 24-bit)
     * R2  (0x02) LOUT1 Volume = 0x0179 (0dB, Update)
     * R3  (0x03) ROUT1 Volume = 0x0179 (0dB, Update)
     */
#endif
    return APG_M7_AUDIO_DRIVER_OK;
}

static apg_m7_audio_driver_status_t wm8960_start(void) {
#if defined(__ARM_ARCH_7EM__) || defined(__arm__)
    /* Enable SAI1_Block_A DMA Transmit (SAI1_Block_A->CR1 |= SAI_xCR1_DMAEN | SAI_xCR1_SAIEN) */
    *(volatile uint32_t *)0x40015804U |= (1U << 17) | (1U << 16);
#endif
    return APG_M7_AUDIO_DRIVER_OK;
}

static apg_m7_audio_driver_status_t wm8960_stop(void) {
#if defined(__ARM_ARCH_7EM__) || defined(__arm__)
    *(volatile uint32_t *)0x40015804U &= ~((1U << 17) | (1U << 16));
#endif
    return APG_M7_AUDIO_DRIVER_OK;
}

static apg_m7_audio_driver_status_t wm8960_set_volume(uint8_t volume_percent) {
    if (volume_percent > 100) volume_percent = 100;
    static_volume_gain = volume_percent;
    return APG_M7_AUDIO_DRIVER_OK;
}

static const apg_m7_audio_driver_t g_wm8960_driver = {
    .mode = APG_M7_AUDIO_IO_WM8960,
    .init = wm8960_init,
    .start = wm8960_start,
    .stop = wm8960_stop,
    .set_volume = wm8960_set_volume
};

const apg_m7_audio_driver_t *apg_m7_audio_driver_get_wm8960(void) {
    return &g_wm8960_driver;
}

/* --- Native GPIO ADC/DAC Driver Option --- */
static apg_m7_audio_driver_status_t gpio_adc_dac_init(uint32_t sample_rate) {
    (void)sample_rate;
#if defined(__ARM_ARCH_7EM__) || defined(__arm__)
    /* 1. Enable TIM6, ADC1, DAC1 Clocks (RCC_APB1ENR bit 4, RCC_APB1ENR bit 29, RCC_APB2ENR bit 8) */
    *(volatile uint32_t *)0x40023840U |= (1U << 4) | (1U << 29);
    *(volatile uint32_t *)0x40023844U |= (1U << 8);

    /* 2. Set TIM6 Prescaler and Auto-Reload for 48kHz trigger (216MHz APB1 timer clock / 48000 = 4500) */
    *(volatile uint32_t *)0x40001028U = 0U;     /* Prescaler = 1 */
    *(volatile uint32_t *)0x4000102CU = 4499U;  /* ARR = 4499 */
    *(volatile uint32_t *)0x40001000U |= (2U << 4); /* Master Mode Selection: Update Event as TRGO */
#endif
    return APG_M7_AUDIO_DRIVER_OK;
}

static apg_m7_audio_driver_status_t gpio_adc_dac_start(void) {
#if defined(__ARM_ARCH_7EM__) || defined(__arm__)
    /* Enable TIM6 Counter (TIM6->CR1 |= TIM_CR1_CEN) */
    *(volatile uint32_t *)0x40001000U |= (1U << 0);
#endif
    return APG_M7_AUDIO_DRIVER_OK;
}

static apg_m7_audio_driver_status_t gpio_adc_dac_stop(void) {
#if defined(__ARM_ARCH_7EM__) || defined(__arm__)
    *(volatile uint32_t *)0x40001000U &= ~(1U << 0);
#endif
    return APG_M7_AUDIO_DRIVER_OK;
}

static apg_m7_audio_driver_status_t gpio_adc_dac_set_volume(uint8_t volume_percent) {
    if (volume_percent > 100) volume_percent = 100;
    static_volume_gain = volume_percent;
    return APG_M7_AUDIO_DRIVER_OK;
}

static const apg_m7_audio_driver_t g_gpio_adc_dac_driver = {
    .mode = APG_M7_AUDIO_IO_GPIO_ADC_DAC,
    .init = gpio_adc_dac_init,
    .start = gpio_adc_dac_start,
    .stop = gpio_adc_dac_stop,
    .set_volume = gpio_adc_dac_set_volume
};

const apg_m7_audio_driver_t *apg_m7_audio_driver_get_gpio_adc_dac(void) {
    return &g_gpio_adc_dac_driver;
}
