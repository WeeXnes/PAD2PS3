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
    USB.VID(USB_CONTROLLER_VID);
    USB.PID(USB_CONTROLLER_PID);
    USB.productName(USB_CONTROLLER_PRODUCT_NAME);
    USB.manufacturerName(USB_CONTROLLER_MANUFACTURER);
    USB.firmwareVersion(0x0100);

    // Initialize official TinyUSB Gamepad class
    s_gamepad.begin();
    USB.begin();

    s_initialized = true;
    return true;
}

bool usb_hid_ready() {
    return s_initialized;
}

bool usb_hid_send(int8_t x, int8_t y, int8_t z, int8_t rz, int8_t rx, int8_t ry, uint8_t hat, uint32_t buttons) {
    if (!s_initialized) {
        return false;
    }
    return s_gamepad.send(x, y, z, rz, rx, ry, hat, buttons);
}
