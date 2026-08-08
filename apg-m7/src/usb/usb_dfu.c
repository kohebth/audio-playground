#include "apg_m7/domain/usb_dfu.h"
#include <string.h>

static apg_m7_usb_dfu_state_t g_dfu_state = DFU_STATE_IDLE;
static apg_m7_usb_dfu_status_t g_dfu_status = {
    .bStatus = DFU_STATUS_OK,
    .bwPollTimeout = {0x00, 0x00, 0x00},
    .bState = DFU_STATE_IDLE,
    .iString = 0
};

void apg_m7_usb_dfu_init(void) {
    g_dfu_state = DFU_STATE_IDLE;
    g_dfu_status.bStatus = DFU_STATUS_OK;
    g_dfu_status.bState = DFU_STATE_IDLE;
}

apg_m7_usb_dfu_state_t apg_m7_usb_dfu_get_state(void) {
    return g_dfu_state;
}

apg_m7_usb_dfu_status_t apg_m7_usb_dfu_get_status(void) {
    g_dfu_status.bState = (uint8_t)g_dfu_state;
    return g_dfu_status;
}

uint8_t apg_m7_usb_dfu_handle_request(uint8_t request, uint16_t value, uint16_t length, const uint8_t *data, uint8_t *out_buffer) {
    (void)value;
    (void)data;

    switch (request) {
        case DFU_GETSTATUS: {
            if (out_buffer && length >= sizeof(apg_m7_usb_dfu_status_t)) {
                apg_m7_usb_dfu_status_t status = apg_m7_usb_dfu_get_status();
                memcpy(out_buffer, &status, sizeof(status));
                if (g_dfu_state == DFU_STATE_DNLOAD_SYNC) {
                    g_dfu_state = DFU_STATE_DNLOAD_IDLE;
                } else if (g_dfu_state == DFU_STATE_MANIFEST_SYNC) {
                    g_dfu_state = DFU_STATE_MANIFEST;
                }
                return (uint8_t)sizeof(status);
            }
            break;
        }

        case DFU_CLRSTATUS: {
            g_dfu_status.bStatus = DFU_STATUS_OK;
            g_dfu_state = DFU_STATE_IDLE;
            return 0;
        }

        case DFU_GETSTATE: {
            if (out_buffer && length >= 1) {
                out_buffer[0] = (uint8_t)g_dfu_state;
                return 1;
            }
            break;
        }

        case DFU_DNLOAD: {
            if (length > 0) {
                g_dfu_state = DFU_STATE_DNLOAD_SYNC;
            } else {
                g_dfu_state = DFU_STATE_MANIFEST_SYNC;
            }
            return 0;
        }

        case DFU_ABORT: {
            g_dfu_state = DFU_STATE_IDLE;
            g_dfu_status.bStatus = DFU_STATUS_OK;
            return 0;
        }

        case DFU_DETACH: {
            g_dfu_state = DFU_STATE_APP_DETACH;
            return 0;
        }

        default:
            g_dfu_status.bStatus = DFU_STATUS_ERR_STALLEDPKT;
            g_dfu_state = DFU_STATE_ERROR;
            break;
    }

    return 0;
}
