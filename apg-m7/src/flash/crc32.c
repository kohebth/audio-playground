#include "apg_m7/domain/crc32.h"

uint32_t apg_m7_crc32(const void *data, size_t length) {
    const uint8_t *byte_data = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFF;
    
    for (size_t i = 0; i < length; i++) {
        crc ^= byte_data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
    }
    
    return crc ^ 0xFFFFFFFF;
}
