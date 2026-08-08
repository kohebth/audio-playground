#include "apg_m7/domain/bsp_hardware.h"
#include <stdio.h>

apg_m7_bsp_status_t apg_m7_bsp_init_clocks(void) {
#if defined(__ARM_ARCH_7EM__) || defined(__arm__)
    /* Enable Cortex-M7 Instruction Cache (SCB->CCR |= SCB_CCR_IC_Msk) */
    *(volatile uint32_t *)0xE000ED14U |= (1U << 17);
    /* Enable Cortex-M7 Data Cache (SCB->CCR |= SCB_CCR_DC_Msk) */
    *(volatile uint32_t *)0xE000ED14U |= (1U << 16);
    __asm__ volatile ("dsb 0xF\nisb 0xF" ::: "memory");
    /* Set FLASH ACR Latency to 7 wait states for 216MHz @ 3.3V */
    *(volatile uint32_t *)0x40023C00U = 0x00000007U;
#endif
    return APG_M7_BSP_OK;
}

apg_m7_bsp_status_t apg_m7_bsp_init_sdram(void) {
#if defined(__ARM_ARCH_7EM__) || defined(__arm__)
    /* 1. Enable AHB3 FMC and GPIO Clock Gates (RCC_AHB3ENR | RCC_AHB1ENR) */
    *(volatile uint32_t *)0x40023838U |= (1U << 0); /* RCC_AHB3ENR |= FMCEN */
    *(volatile uint32_t *)0x40023830U |= 0x000000FFU; /* GPIOA..GPIOH Clocks */

    /* 2. FMC SDRAM Control Register (FMC_SDCR1 at 0xA0000140): CAS=2, 4 Banks, 12-bit Row, 8-bit Col, 16-bit Bus */
    *(volatile uint32_t *)0xA0000140U = 0x00001800U | (2U << 7) | (1U << 4) | (1U << 2) | (1U << 0);
    /* 3. FMC SDRAM Timing Register (FMC_SDTR1 at 0xA0000144) */
    *(volatile uint32_t *)0xA0000144U = (1U << 24) | (1U << 20) | (1U << 16) | (5U << 12) | (1U << 8) | (5U << 4) | (1U << 0);
    /* 4. Issue SDRAM Mode Command Sequence: Clock Configuration Enable -> Precharge All -> Auto-Refresh -> Load Mode */
    *(volatile uint32_t *)0xA0000150U = 0x00000001U; /* Clock Config Enable */
    *(volatile uint32_t *)0xA0000150U = 0x00000002U; /* Precharge All */
    *(volatile uint32_t *)0xA0000150U = (2U << 5) | 0x00000003U; /* Auto Refresh (2 cycles) */
    *(volatile uint32_t *)0xA0000150U = (0x0020U << 9) | 0x00000004U; /* Load Mode Register (Burst 1, CAS 2) */
    /* 5. Set Refresh Rate Counter Register (FMC_SDRTR at 0xA0000154 = 1386) */
    *(volatile uint32_t *)0xA0000154U = (APG_M7_FMC_SDRAM_REFRESH_RATE << 1);
#endif
    return APG_M7_BSP_OK;
}

apg_m7_bsp_status_t apg_m7_bsp_init_audio_sai(uint32_t sample_rate) {
    (void)sample_rate;
#if defined(__ARM_ARCH_7EM__) || defined(__arm__)
    /* 1. Enable SAI1 and I2C1 Peripheral Clocks (RCC_APB2ENR bit 22, RCC_APB1ENR bit 21) */
    *(volatile uint32_t *)0x40023844U |= (1U << 22); /* SAI1 */
    *(volatile uint32_t *)0x40023840U |= (1U << 21); /* I2C1 */

    /* 2. Configure PLLI2S for 48kHz audio clock: PLLI2SN=192, PLLI2SQ=2, PLLI2SR=2 */
    *(volatile uint32_t *)0x40023884U = (192U << 6) | (2U << 24) | (2U << 28);
    *(volatile uint32_t *)0x40023800U |= (1U << 26); /* PLLI2SON = 1 */

    /* 3. Configure SAI1_Block_A CR1 (0x40015804): Master Transmitter, Free Protocol I2S, 24-bit Data */
    *(volatile uint32_t *)0x40015804U = (0U << 0) | (2U << 2) | (4U << 5) | (1U << 8);
#endif
    return APG_M7_BSP_OK;
}

apg_m7_bsp_status_t apg_m7_bsp_init_dma_buffers(void) {
#if defined(__ARM_ARCH_7EM__) || defined(__arm__)
    /* Enable DMA2 Clock (RCC_AHB1ENR bit 22) */
    *(volatile uint32_t *)0x40023830U |= (1U << 22);
    /* Configure DMA2 Stream 1 for SAI1_A Double-Buffered Transmit (Double Buffer Mode DBM=1) */
    *(volatile uint32_t *)0x40026418U |= (1U << 18) | (1U << 10) | (1U << 8) | (1U << 6);
#endif
    return APG_M7_BSP_OK;
}

void apg_m7_bsp_clean_dcache(const void *addr, uint32_t size) {
#if defined(__ARM_ARCH_7EM__) || defined(__arm__)
    /* Cortex-M7 SCB_CleanDCache_by_Addr implementation (32-byte cache lines) */
    uint32_t op_addr = (uint32_t)addr & ~0x1FU;
    int32_t op_size = size + ((uint32_t)addr & 0x1FU);
    __asm__ volatile ("dsb 0xF" ::: "memory");
    while (op_size > 0) {
        /* SCB->DCCMVAC address: 0xE000EF68 */
        *(volatile uint32_t *)0xE000EF68U = op_addr;
        op_addr += 32U;
        op_size -= 32;
    }
    __asm__ volatile ("dsb 0xF\nisb 0xF" ::: "memory");
#else
    (void)addr;
    (void)size;
    __asm__ volatile("" ::: "memory");
#endif
}

void apg_m7_bsp_invalidate_dcache(const void *addr, uint32_t size) {
#if defined(__ARM_ARCH_7EM__) || defined(__arm__)
    /* Cortex-M7 SCB_InvalidateDCache_by_Addr implementation (32-byte cache lines) */
    uint32_t op_addr = (uint32_t)addr & ~0x1FU;
    int32_t op_size = size + ((uint32_t)addr & 0x1FU);
    __asm__ volatile ("dsb 0xF" ::: "memory");
    while (op_size > 0) {
        /* SCB->DCIMVAC address: 0xE000EF64 */
        *(volatile uint32_t *)0xE000EF64U = op_addr;
        op_addr += 32U;
        op_size -= 32;
    }
    __asm__ volatile ("dsb 0xF\nisb 0xF" ::: "memory");
#else
    (void)addr;
    (void)size;
    __asm__ volatile("" ::: "memory");
#endif
}
