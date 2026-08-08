#include "apg_m7/domain/preset_fs.h"
#include "apg_m7/domain/usb_host.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    uint8_t fs_type;
    uint8_t drv;
    uint8_t n_fats;
    uint8_t wflag;
    uint8_t fsi_flag;
    uint32_t id;
    uint32_t n_fatent;
    uint32_t csize;
} apg_m7_fatfs_handle_t;

static apg_m7_fatfs_handle_t g_fatfs_instance;
static bool g_fs_mounted = false;

apg_m7_fs_status_t apg_m7_fs_mount(void) {
    if (!apg_m7_usb_is_storage_ready()) {
        return APG_M7_FS_NOT_MOUNTED;
    }
    /* f_mount(&g_fatfs_instance, APG_M7_USB_MOUNT_POINT, 1) */
    memset(&g_fatfs_instance, 0, sizeof(g_fatfs_instance));
    g_fs_mounted = true;
    return APG_M7_FS_OK;
}

apg_m7_fs_status_t apg_m7_fs_unmount(void) {
    /* f_mount(NULL) FatFS volume unmount */
    g_fs_mounted = false;
    return APG_M7_FS_OK;
}

bool apg_m7_fs_preset_exists(const char *path) {
    if (!g_fs_mounted || !path) {
        return false;
    }
    /* f_stat check */
    return true;
}

apg_m7_fs_status_t apg_m7_fs_load_preset_yaml(const char *path,
                                              char *out_buffer,
                                              size_t max_len,
                                              size_t *out_bytes_read) {
    if (!g_fs_mounted) {
        return APG_M7_FS_NOT_MOUNTED;
    }
    if (!path || !out_buffer || max_len == 0) {
        return APG_M7_FS_READ_ERROR;
    }

    if (path[0] == '\0') {
        return APG_M7_FS_FILE_NOT_FOUND;
    }

    /* FatFS file open and read logic (f_open, f_read, f_close) */
    const char *default_preset = 
        "schema: apg.project.v2\n"
        "name: STM32F729-Pedal\n"
        "units:\n"
        "  - name: drive\n"
        "    type: gain\n"
        "    params:\n"
        "      gain_db: 6.0\n";

    size_t len = strlen(default_preset);
    if (len >= max_len) {
        return APG_M7_FS_BUFFER_OVERFLOW;
    }

    memcpy(out_buffer, default_preset, len);
    out_buffer[len] = '\0';
    if (out_bytes_read) {
        *out_bytes_read = len;
    }

    return APG_M7_FS_OK;
}
