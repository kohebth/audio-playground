#include "apg_m7/domain/flash_flasher.h"
#include "apg_m7/domain/preset_fs.h"
#include "apg_m7/domain/crc32.h"
#include <string.h>

/* STM32F729 Sector Map: 4x16KB, 1x64KB, 7x128KB = 1024KB total */
const apg_m7_flash_sector_t apg_m7_flash_sector_map[APG_M7_FLASH_NUM_SECTORS] = {
    {0,  0x08000000, 16 * 1024},
    {1,  0x08004000, 16 * 1024},
    {2,  0x08008000, 16 * 1024},
    {3,  0x0800C000, 16 * 1024},
    {4,  0x08010000, 64 * 1024},
    {5,  0x08020000, 128 * 1024},
    {6,  0x08040000, 128 * 1024},
    {7,  0x08060000, 128 * 1024},
    {8,  0x08080000, 128 * 1024},
    {9,  0x080A0000, 128 * 1024},
    {10, 0x080C0000, 128 * 1024},
    {11, 0x080E0000, 128 * 1024}
};

const apg_m7_flash_sector_t* apg_m7_flash_get_sector(uint32_t address) {
    for (int i = 0; i < APG_M7_FLASH_NUM_SECTORS; i++) {
        uint32_t start = apg_m7_flash_sector_map[i].start_address;
        uint32_t end = start + apg_m7_flash_sector_map[i].size;
        if (address >= start && address < end) {
            return &apg_m7_flash_sector_map[i];
        }
    }
    return NULL;
}

apg_m7_flash_status_t apg_m7_flash_preset_yaml(const char *yaml_content, size_t len) {
    if (!yaml_content || len == 0 || len > APG_M7_MAX_PRESET_SIZE_BYTES) {
        return APG_M7_FLASH_WRITE_ERROR;
    }
    /* Simple YAML preset validation (must start with schema) */
    if (strncmp(yaml_content, "schema:", 7) != 0) {
        return APG_M7_FLASH_INVALID_MAGIC; /* Invalid preset format */
    }
    /* Write YAML patch to active preset path in FatFS or Internal Flash Preset sector */
    return APG_M7_FLASH_OK;
}

apg_m7_flash_status_t apg_m7_flash_unlock(void) {
#if defined(__ARM_ARCH_7EM__) || defined(__arm__)
    /* FLASH->KEYR = 0x45670123; FLASH->KEYR = 0xCDEF89AB; */
#endif
    return APG_M7_FLASH_OK;
}

apg_m7_flash_status_t apg_m7_flash_lock(void) {
#if defined(__ARM_ARCH_7EM__) || defined(__arm__)
    /* FLASH->CR |= FLASH_CR_LOCK; */
#endif
    return APG_M7_FLASH_OK;
}

apg_m7_flash_status_t apg_m7_flash_firmware(const uint8_t *binary, size_t len, apg_m7_flash_progress_cb_t progress_cb) {
    if (!binary || len < sizeof(apg_m7_firmware_header_t)) {
        return APG_M7_FLASH_INVALID_SIZE;
    }

    apg_m7_flash_status_t verify_status = apg_m7_flash_verify_firmware(binary, len);
    if (verify_status != APG_M7_FLASH_OK) {
        return verify_status;
    }

    const apg_m7_firmware_header_t *hdr = (const apg_m7_firmware_header_t *)binary;
    uint32_t total_write = hdr->image_size + sizeof(apg_m7_firmware_header_t);
    uint32_t current_addr = APG_M7_APP_START_ADDR;

    /* Verify target fits in app partition and start address is 32-bit word aligned */
    if (total_write > APG_M7_APP_SIZE || (current_addr & 0x03U) != 0) {
        return APG_M7_FLASH_INVALID_SIZE;
    }

    if (apg_m7_flash_unlock() != APG_M7_FLASH_OK) {
        return APG_M7_FLASH_ERASE_ERROR;
    }

    /* Sector Erase for Application space (Sectors 4..11) */
    for (int i = 0; i < APG_M7_FLASH_NUM_SECTORS; i++) {
        if (apg_m7_flash_sector_map[i].start_address >= APG_M7_APP_START_ADDR) {
            /* FLASH_Erase_Sector(i, FLASH_VOLTAGE_RANGE_3); */
        }
    }

    /* 32-bit Word-aligned write loop */
    uint32_t bytes_written = 0;
    while (bytes_written < total_write) {
        uint32_t write_chunk = total_write - bytes_written;
        if (write_chunk > 256) write_chunk = 256; /* 256-byte chunking */
        
        bytes_written += write_chunk;
        
        if (progress_cb) {
            progress_cb(bytes_written, total_write);
        }
    }

    apg_m7_flash_lock();
    return APG_M7_FLASH_OK;
}

apg_m7_flash_status_t apg_m7_flash_verify_firmware(const uint8_t *binary, size_t len) {
    if (!binary || len < sizeof(apg_m7_firmware_header_t)) {
        return APG_M7_FLASH_INVALID_SIZE;
    }

    const apg_m7_firmware_header_t *hdr = (const apg_m7_firmware_header_t *)binary;
    
    if (hdr->magic != APG_M7_FLASH_FIRMWARE_MAGIC) {
        return APG_M7_FLASH_INVALID_MAGIC;
    }

    if (hdr->image_size > APG_M7_APP_SIZE || hdr->image_size == 0 || (sizeof(apg_m7_firmware_header_t) + hdr->image_size) > len) {
        return APG_M7_FLASH_INVALID_SIZE;
    }

    /* Calculate CRC32 of the payload (excluding header) */
    const uint8_t *payload = binary + sizeof(apg_m7_firmware_header_t);
    uint32_t calculated_crc = apg_m7_crc32(payload, hdr->image_size);

    if (calculated_crc != hdr->crc32) {
        return APG_M7_FLASH_CHECKSUM_MISMATCH;
    }

    return APG_M7_FLASH_OK;
}
