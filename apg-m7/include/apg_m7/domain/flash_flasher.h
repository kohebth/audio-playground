#ifndef APG_M7_FLASH_FLASHER_H
#define APG_M7_FLASH_FLASHER_H

#include "apg_m7/system_config.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APG_M7_FLASH_OK = 0,
    APG_M7_FLASH_INVALID_MAGIC,
    APG_M7_FLASH_CHECKSUM_MISMATCH,
    APG_M7_FLASH_ERASE_ERROR,
    APG_M7_FLASH_WRITE_ERROR,
    APG_M7_FLASH_BUSY,
    APG_M7_FLASH_INVALID_ADDRESS,
    APG_M7_FLASH_INVALID_SIZE
} apg_m7_flash_status_t;

#define APG_M7_FLASH_FIRMWARE_MAGIC     0x4150474D /* "APGM" */

/* Firmware header with target_board */
typedef struct {
    uint32_t magic;              /* APG_M7_FLASH_FIRMWARE_MAGIC */
    uint32_t version;            /* Firmware revision */
    uint32_t image_size;         /* Size of binary in bytes (excluding header) */
    uint32_t crc32;              /* Checksum of the image (excluding header) */
    uint32_t target_board;       /* Target board identifier */
    uint32_t reserved[3];        /* Reserved for future use (padding to 32 bytes) */
} apg_m7_firmware_header_t;

/* STM32F729 Flash Sector Map Definition */
typedef struct {
    uint8_t sector_num;
    uint32_t start_address;
    uint32_t size;
} apg_m7_flash_sector_t;

#define APG_M7_FLASH_BASE_ADDRESS    0x08000000
#define APG_M7_FLASH_NUM_SECTORS     12

/* Bootloader uses sectors 0-3 (64KB total) */
#define APG_M7_BOOTLOADER_START_ADDR 0x08000000
#define APG_M7_BOOTLOADER_SIZE       (64 * 1024)

/* Application uses sectors 4-11 (960KB total) */
#define APG_M7_APP_START_ADDR        0x08010000
#define APG_M7_APP_SIZE              (960 * 1024)

extern const apg_m7_flash_sector_t apg_m7_flash_sector_map[APG_M7_FLASH_NUM_SECTORS];

/* Callback for flash operations progress */
typedef void (*apg_m7_flash_progress_cb_t)(uint32_t current_bytes, uint32_t total_bytes);

/* Sector flash & update methods */
apg_m7_flash_status_t apg_m7_flash_preset_yaml(const char *yaml_content, size_t len);
apg_m7_flash_status_t apg_m7_flash_firmware(const uint8_t *binary, size_t len, apg_m7_flash_progress_cb_t progress_cb);
apg_m7_flash_status_t apg_m7_flash_verify_firmware(const uint8_t *binary, size_t len);

/* Flash Hardware Control */
apg_m7_flash_status_t apg_m7_flash_unlock(void);
apg_m7_flash_status_t apg_m7_flash_lock(void);

/* Helper function to get sector from address */
const apg_m7_flash_sector_t* apg_m7_flash_get_sector(uint32_t address);

#ifdef __cplusplus
}
#endif

#endif /* APG_M7_FLASH_FLASHER_H */
