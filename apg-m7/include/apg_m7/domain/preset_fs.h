#ifndef APG_M7_PRESET_FS_H
#define APG_M7_PRESET_FS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APG_M7_FS_OK = 0,
    APG_M7_FS_NOT_MOUNTED,
    APG_M7_FS_FILE_NOT_FOUND,
    APG_M7_FS_READ_ERROR,
    APG_M7_FS_BUFFER_OVERFLOW
} apg_m7_fs_status_t;

apg_m7_fs_status_t apg_m7_fs_mount(void);
apg_m7_fs_status_t apg_m7_fs_unmount(void);

apg_m7_fs_status_t apg_m7_fs_load_preset_yaml(const char *path,
                                              char *out_buffer,
                                              size_t max_len,
                                              size_t *out_bytes_read);

bool apg_m7_fs_preset_exists(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* APG_M7_PRESET_FS_H */
