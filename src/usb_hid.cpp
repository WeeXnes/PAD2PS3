#include "usb_hid.h"
#include <Arduino.h>
#include "USB.h"
#include "USBHIDGamepad.h"

static USBHIDGamepad s_gamepad;
static bool s_initialized = false;

bool usb_hid_init() {
    if (s_initialized) {
        return true;
    }

    // Configure USB Device metadata
    bool vid_ok = USB.VID(USB_CONTROLLER_VID);
    bool pid_ok = USB.PID(USB_CONTROLLER_PID);
    bool prod_ok = USB.productName(USB_CONTROLLER_PRODUCT_NAME);
    bool mfr_ok = USB.manufacturerName(USB_CONTROLLER_MANUFACTURER);
    bool fw_ok = USB.firmwareVersion(0x0100);

    // Initialize official TinyUSB Gamepad class
    s_gamepad.begin();
    bool usb_ok = USB.begin();

    Serial.printf("[USB] VID: %d, PID: %d, Prod: %d, Mfr: %d, FW: %d, USB begin: %d\n",
                  vid_ok, pid_ok, prod_ok, mfr_ok, fw_ok, usb_ok);

    s_initialized = true;
    return true;
}

bool usb_hid_ready() {
    return s_initialized && (bool)USB;
}

bool usb_hid_send(int8_t x, int8_t y, int8_t z, int8_t rz, int8_t rx, int8_t ry, uint8_t hat, uint32_t buttons) {
    if (!s_initialized || !((bool)USB)) {
        return false;
    }
    return s_gamepad.send(x, y, z, rz, rx, ry, hat, buttons);
}
