#include "apg_m7/domain/usb_host.h"

static apg_m7_usb_state_t g_usb_state = APG_M7_USB_DISCONNECTED;

void apg_m7_usb_init(void) {
    /* Initialize USB OTG HS peripheral in Host Mode */
    g_usb_state = APG_M7_USB_DISCONNECTED;
}

void apg_m7_usb_process(void) {
    /* USB Host State Machine background task (USBH_Process) */
    /* Transition states based on USB Flash Drive connection events */
    if (g_usb_state == APG_M7_USB_DISCONNECTED) {
        /* Mock state transition for development / simulation */
        g_usb_state = APG_M7_USB_MOUNTED;
    }
}

apg_m7_usb_state_t apg_m7_usb_get_state(void) {
    return g_usb_state;
}

bool apg_m7_usb_is_storage_ready(void) {
    return (g_usb_state == APG_M7_USB_MOUNTED);
}
