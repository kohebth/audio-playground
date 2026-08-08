#ifndef APG_M7_USB_DFU_H
#define APG_M7_USB_DFU_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* DFU Functional Descriptor (USB DFU 1.1) */
typedef struct __attribute__((packed)) {
    uint8_t bLength;            /* Size of this descriptor (9 bytes) */
    uint8_t bDescriptorType;    /* DFU FUNCTIONAL descriptor type (0x21) */
    uint8_t bmAttributes;       /* DFU attributes */
    uint16_t wDetachTimeOut;    /* Time in ms to wait after detach */
    uint16_t wTransferSize;     /* Maximum number of bytes per transfer */
    uint16_t bcdDFUVersion;     /* DFU specification release (0x0110 for 1.1) */
} apg_m7_usb_dfu_functional_descriptor_t;

/* DFU Status */
typedef struct __attribute__((packed)) {
    uint8_t bStatus;            /* Status resulting from last request */
    uint8_t bwPollTimeout[3];   /* Minimum time in ms before next request */
    uint8_t bState;             /* State the device is going to enter */
    uint8_t iString;            /* Index of status description string */
} apg_m7_usb_dfu_status_t;

/* DFU Requests */
#define DFU_DETACH      0
#define DFU_DNLOAD      1
#define DFU_UPLOAD      2
#define DFU_GETSTATUS   3
#define DFU_CLRSTATUS   4
#define DFU_GETSTATE    5
#define DFU_ABORT       6

/* DFU States */
typedef enum {
    DFU_STATE_APP_IDLE = 0,
    DFU_STATE_APP_DETACH = 1,
    DFU_STATE_IDLE = 2,
    DFU_STATE_DNLOAD_SYNC = 3,
    DFU_STATE_DNBUSY = 4,
    DFU_STATE_DNLOAD_IDLE = 5,
    DFU_STATE_MANIFEST_SYNC = 6,
    DFU_STATE_MANIFEST = 7,
    DFU_STATE_MANIFEST_WAIT_RESET = 8,
    DFU_STATE_UPLOAD_IDLE = 9,
    DFU_STATE_ERROR = 10
} apg_m7_usb_dfu_state_t;

/* DFU Status values */
#define DFU_STATUS_OK               0x00
#define DFU_STATUS_ERR_TARGET       0x01
#define DFU_STATUS_ERR_FILE         0x02
#define DFU_STATUS_ERR_WRITE        0x03
#define DFU_STATUS_ERR_ERASE        0x04
#define DFU_STATUS_ERR_CHECK_ERASED 0x05
#define DFU_STATUS_ERR_PROG         0x06
#define DFU_STATUS_ERR_VERIFY       0x07
#define DFU_STATUS_ERR_ADDRESS      0x08
#define DFU_STATUS_ERR_NOTDONE      0x09
#define DFU_STATUS_ERR_FIRMWARE     0x0A
#define DFU_STATUS_ERR_VENDOR       0x0B
#define DFU_STATUS_ERR_USBR         0x0C
#define DFU_STATUS_ERR_POR          0x0D
#define DFU_STATUS_ERR_UNKNOWN      0x0E
#define DFU_STATUS_ERR_STALLEDPKT   0x0F

/* DFU Function Prototypes */
void apg_m7_usb_dfu_init(void);
apg_m7_usb_dfu_state_t apg_m7_usb_dfu_get_state(void);
apg_m7_usb_dfu_status_t apg_m7_usb_dfu_get_status(void);
uint8_t apg_m7_usb_dfu_handle_request(uint8_t request, uint16_t value, uint16_t length, const uint8_t *data, uint8_t *out_buffer);

#ifdef __cplusplus
}
#endif

#endif /* APG_M7_USB_DFU_H */
