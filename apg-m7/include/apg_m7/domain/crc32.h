#ifndef APG_M7_CRC32_H
#define APG_M7_CRC32_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t apg_m7_crc32(const void *data, size_t length);

#ifdef __cplusplus
}
#endif

#endif /* APG_M7_CRC32_H */
