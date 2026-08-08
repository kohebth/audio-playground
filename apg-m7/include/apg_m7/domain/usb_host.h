#ifndef APG_M7_USB_HOST_H
#define APG_M7_USB_HOST_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APG_M7_USB_DISCONNECTED = 0,
    APG_M7_USB_CONNECTED,
    APG_M7_USB_MOUNTED,
    APG_M7_USB_ERROR
} apg_m7_usb_state_t;

void apg_m7_usb_init(void);
void apg_m7_usb_process(void);
apg_m7_usb_state_t apg_m7_usb_get_state(void);
bool apg_m7_usb_is_storage_ready(void);

#ifdef __cplusplus
}
#endif

#endif /* APG_M7_USB_HOST_H */
