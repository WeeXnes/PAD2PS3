#include "usb_hid.h"
#include <Arduino.h>
#include "USB.h"
#include "USBHID.h"

// ---------------------------------------------------------------------------
// Official Sony PlayStation 3 DualShock 3 HID Report Descriptor (176 Bytes)
// Includes genuine PS3 report format (Report 0x01: 49 bytes, Output: 48 bytes)
// plus Feature Reports 0xF2 (17 bytes: Bluetooth MAC) and 0xF5 (8 bytes: operational mode)
// to satisfy Linux kernel hid-sony driver and the PS3 console handshake.
// ---------------------------------------------------------------------------
static const uint8_t s_sony_ds3_report_descriptor[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
    0x09, 0x04,        // Usage (Joystick)
    0xa1, 0x01,        // Collection (Application)
    0xa1, 0x02,        //   Collection (Logical)
    0x85, 0x01,        //     Report ID (1)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x01,        //     Report Count (1)
    0x15, 0x00,        //     Logical Minimum (0)
    0x26, 0xff, 0x00,  //     Logical Maximum (255)
    0x81, 0x03,        //     Input (Const,Var,Abs)
    0x75, 0x01,        //     Report Size (1)
    0x95, 0x13,        //     Report Count (19)
    0x15, 0x00,        //     Logical Minimum (0)
    0x25, 0x01,        //     Logical Maximum (1)
    0x35, 0x00,        //     Physical Minimum (0)
    0x45, 0x01,        //     Physical Maximum (1)
    0x05, 0x09,        //     Usage Page (Button)
    0x19, 0x01,        //     Usage Minimum (0x01)
    0x29, 0x13,        //     Usage Maximum (0x13)
    0x81, 0x02,        //     Input (Data,Var,Abs)
    0x75, 0x01,        //     Report Size (1)
    0x95, 0x0d,        //     Report Count (13)
    0x06, 0x00, 0xff,  //     Usage Page (Vendor Defined 0xFF00)
    0x81, 0x03,        //     Input (Const,Var,Abs)
    0x15, 0x00,        //     Logical Minimum (0)
    0x26, 0xff, 0x00,  //     Logical Maximum (255)
    0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
    0x09, 0x01,        //     Usage (Pointer)
    0xa1, 0x00,        //     Collection (Physical)
    0x75, 0x08,        //       Report Size (8)
    0x95, 0x04,        //       Report Count (4)
    0x35, 0x00,        //       Physical Minimum (0)
    0x46, 0xff, 0x00,  //       Physical Maximum (255)
    0x09, 0x30,        //       Usage (X)
    0x09, 0x31,        //       Usage (Y)
    0x09, 0x32,        //       Usage (Z)
    0x09, 0x35,        //       Usage (Rz)
    0x81, 0x02,        //       Input (Data,Var,Abs)
    0xc0,              //     End Collection
    0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x27,        //     Report Count (39)
    0x09, 0x01,        //     Usage (Pointer)
    0x81, 0x02,        //     Input (Data,Var,Abs)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x30,        //     Report Count (48)
    0x09, 0x01,        //     Usage (Pointer)
    0x91, 0x02,        //     Output (Data,Var,Abs)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x30,        //     Report Count (48)
    0x09, 0x01,        //     Usage (Pointer)
    0xb1, 0x02,        //     Feature (Data,Var,Abs)
    0xc0,              //   End Collection
    0xa1, 0x02,        //   Collection (Logical)
    0x85, 0x02,        //     Report ID (2)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x30,        //     Report Count (48)
    0x09, 0x01,        //     Usage (Pointer)
    0xb1, 0x02,        //     Feature (Data,Var,Abs)
    0xc0,              //   End Collection
    0xa1, 0x02,        //   Collection (Logical)
    0x85, 0xee,        //     Report ID (238)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x30,        //     Report Count (48)
    0x09, 0x01,        //     Usage (Pointer)
    0xb1, 0x02,        //     Feature (Data,Var,Abs)
    0xc0,              //   End Collection
    0xa1, 0x02,        //   Collection (Logical)
    0x85, 0xef,        //     Report ID (239)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x30,        //     Report Count (48)
    0x09, 0x01,        //     Usage (Pointer)
    0xb1, 0x02,        //     Feature (Data,Var,Abs)
    0xc0,              //   End Collection
    0xa1, 0x02,        //   Collection (Logical)
    0x85, 0xf2,        //     Report ID (242 = 0xF2)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x11,        //     Report Count (17 bytes)
    0x09, 0x01,        //     Usage (Pointer)
    0xb1, 0x02,        //     Feature (Data,Var,Abs)
    0xc0,              //   End Collection
    0xa1, 0x02,        //   Collection (Logical)
    0x85, 0xf5,        //     Report ID (245 = 0xF5)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x08,        //     Report Count (8 bytes)
    0x09, 0x01,        //     Usage (Pointer)
    0xb1, 0x02,        //     Feature (Data,Var,Abs)
    0xc0,              //   End Collection
    0xc0               // End Collection
};

static_assert(sizeof(PS3NativeInputReport) == 48, "PS3NativeInputReport must be exactly 48 bytes");

class PS3USBHIDDevice : public USBHIDDevice {
public:
    PS3USBHIDDevice() {}

    uint16_t _onGetDescriptor(uint8_t* dst) override {
        memcpy(dst, s_sony_ds3_report_descriptor, sizeof(s_sony_ds3_report_descriptor));
        return sizeof(s_sony_ds3_report_descriptor);
    }

    uint16_t _onGetFeature(uint8_t report_id, uint8_t* buffer, uint16_t reqlen) override {
        Serial.printf("[USB HID] Get Feature Report: ID 0x%02X, req len %u\n", report_id, reqlen);
        if (report_id == 0xF2) {
            uint16_t copy_len = (reqlen < 17) ? reqlen : 17;
            memset(buffer, 0, copy_len);
            buffer[0] = 0xF2;
            // Simulated Bluetooth MAC Address at bytes 4..9 (00:1A:80:AA:BB:CC)
            buffer[4] = 0x00;
            buffer[5] = 0x1A;
            buffer[6] = 0x80;
            buffer[7] = 0xAA;
            buffer[8] = 0xBB;
            buffer[9] = 0xCC;
            return copy_len;
        }
        if (report_id == 0xF5) {
            uint16_t copy_len = (reqlen < 8) ? reqlen : 8;
            memset(buffer, 0, copy_len);
            buffer[0] = 0xF5;
            return copy_len;
        }
        // Respond to ANY requested report ID with valid payload so TinyUSB never stalls
        if (reqlen > 0) {
            memset(buffer, 0, reqlen);
            buffer[0] = report_id;
            return reqlen;
        }
        return 0;
    }

    void _onSetFeature(uint8_t report_id, const uint8_t* buffer, uint16_t len) override {
        Serial.printf("[USB HID] Set Feature Report: ID 0x%02X, len %u\n", report_id, len);
    }

    void _onOutput(uint8_t report_id, const uint8_t* buffer, uint16_t len) override {
        Serial.printf("[USB HID] Output Report: ID 0x%02X, len %u\n", report_id, len);
    }
};

static USBHID s_hid;
static PS3USBHIDDevice s_device;
static bool s_initialized = false;

bool usb_hid_init() {
    if (s_initialized) {
        return true;
    }

    // Configure authentic Sony PlayStation 3 Device Metadata matching genuine DS3
    bool vid_ok  = USB.VID(USB_CONTROLLER_VID);
    bool pid_ok  = USB.PID(USB_CONTROLLER_PID);
    bool prod_ok = USB.productName(USB_CONTROLLER_PRODUCT_NAME);
    bool mfr_ok  = USB.manufacturerName(USB_CONTROLLER_MANUFACTURER);
    USB.serialNumber("");                 // Genuine PS3 controller has iSerial = 0
    bool fw_ok   = USB.firmwareVersion(0x0100); // bcdDevice = 1.00
    USB.usbVersion(0x0200);               // bcdUSB = 2.00
    USB.usbPower(500);                    // MaxPower = 500mA
    USB.usbClass(0);                      // bDeviceClass = 0 (Defined at interface level)
    USB.usbSubClass(0);                   // bDeviceSubClass = 0
    USB.usbProtocol(0);                   // bDeviceProtocol = 0
    USB.usbAttributes(0x80);              // bmAttributes = 0x80 (Bus Powered, NOT Self-Powered)
    USB.webUSB(false);                    // Disable WebUSB to prevent BOS descriptors

    if (!s_hid.addDevice(&s_device, sizeof(s_sony_ds3_report_descriptor))) {
        Serial.println("[USB HID] ERROR: addDevice failed!");
        return false;
    }

    s_hid.begin();
    bool usb_ok = USB.begin();

    Serial.printf("[USB] VID: %d, PID: %d, Prod: %d, Mfr: %d, FW: %d, USB begin: %d\n",
                  vid_ok, pid_ok, prod_ok, mfr_ok, fw_ok, usb_ok);

    s_initialized = true;
    return true;
}

bool usb_hid_ready() {
    return s_initialized && ((bool)USB);
}

bool usb_hid_send(int8_t x, int8_t y, int8_t z, int8_t rz, int8_t rx, int8_t ry, uint8_t hat, uint32_t buttons) {
    if (!s_initialized || !((bool)USB)) {
        return false;
    }

    // Avoid blocking or queueing when endpoint is still busy with previous report
    if (!s_hid.ready()) {
        return false;
    }

    PS3NativeInputReport rep;
    memset(&rep, 0, sizeof(rep));

    // Convert -127..127 to 0..255 for PS3 sticks
    rep.left_x  = (uint8_t)(x + 128);
    rep.left_y  = (uint8_t)(y + 128);
    rep.right_x = (uint8_t)(z + 128);
    rep.right_y = (uint8_t)(rz + 128);

    // D-Pad Hat Switch (1..8)
    // Bit mapping in sixaxis buttons1: Up=bit 4, Right=bit 5, Down=bit 6, Left=bit 7
    switch (hat) {
        case 1: rep.buttons1 |= (1 << 4); break; // Up
        case 2: rep.buttons1 |= (1 << 4) | (1 << 5); break; // Up-Right
        case 3: rep.buttons1 |= (1 << 5); break; // Right
        case 4: rep.buttons1 |= (1 << 5) | (1 << 6); break; // Down-Right
        case 5: rep.buttons1 |= (1 << 6); break; // Down
        case 6: rep.buttons1 |= (1 << 6) | (1 << 7); break; // Down-Left
        case 7: rep.buttons1 |= (1 << 7); break; // Left
        case 8: rep.buttons1 |= (1 << 7) | (1 << 4); break; // Up-Left
        default: break;
    }
    if (rep.buttons1 & (1 << 4)) rep.press_up = 255;
    if (rep.buttons1 & (1 << 5)) rep.press_right = 255;
    if (rep.buttons1 & (1 << 6)) rep.press_down = 255;
    if (rep.buttons1 & (1 << 7)) rep.press_left = 255;

    // Digital Buttons Mapping to Sixaxis layout
    // buttons bit 0: CROSS -> buttons2 bit 6
    if (buttons & (1 << 0)) { rep.buttons2 |= (1 << 6); rep.press_cross = 255; }
    // buttons bit 1: CIRCLE -> buttons2 bit 5
    if (buttons & (1 << 1)) { rep.buttons2 |= (1 << 5); rep.press_circle = 255; }
    // buttons bit 2: SQUARE -> buttons2 bit 7
    if (buttons & (1 << 2)) { rep.buttons2 |= (1 << 7); rep.press_square = 255; }
    // buttons bit 3: TRIANGLE -> buttons2 bit 4
    if (buttons & (1 << 3)) { rep.buttons2 |= (1 << 4); rep.press_triangle = 255; }
    // buttons bit 4: L1 -> buttons2 bit 2
    if (buttons & (1 << 4)) { rep.buttons2 |= (1 << 2); rep.press_l1 = 255; }
    // buttons bit 5: R1 -> buttons2 bit 3
    if (buttons & (1 << 5)) { rep.buttons2 |= (1 << 3); rep.press_r1 = 255; }
    // buttons bit 6: L2 -> buttons2 bit 0
    if (buttons & (1 << 6)) { rep.buttons2 |= (1 << 0); }
    // buttons bit 7: R2 -> buttons2 bit 1
    if (buttons & (1 << 7)) { rep.buttons2 |= (1 << 1); }
    // buttons bit 8: SELECT -> buttons1 bit 0
    if (buttons & (1 << 8)) { rep.buttons1 |= (1 << 0); }
    // buttons bit 9: START -> buttons1 bit 3
    if (buttons & (1 << 9)) { rep.buttons1 |= (1 << 3); }
    // buttons bit 10: L3 -> buttons1 bit 1
    if (buttons & (1 << 10)) { rep.buttons1 |= (1 << 1); }
    // buttons bit 11: R3 -> buttons1 bit 2
    if (buttons & (1 << 11)) { rep.buttons1 |= (1 << 2); }
    // buttons bit 12: PS -> ps_button bit 0
    if (buttons & (1 << 12)) { rep.ps_button |= (1 << 0); }

    // Analog Triggers (rx and ry: -127..127 -> 0..255)
    rep.press_l2 = (uint8_t)(rx + 128);
    rep.press_r2 = (uint8_t)(ry + 128);
    if (rep.press_l2 > 20) rep.buttons2 |= (1 << 0);
    if (rep.press_r2 > 20) rep.buttons2 |= (1 << 1);

    // Battery status (0xEE = charging / 100%) and Connection (0x02 = USB)
    rep.charge = 0xEE;
    rep.connection = 0x02;

    // Neutral Accelerometer values (512 = 0x0200)
    rep.accel_x = 512;
    rep.accel_y = 512;
    rep.accel_z = 512;
    rep.gyro_z  = 512;

    // Send 48 bytes with Report ID 0x01 (TinyUSB prepends ID making 49 bytes on wire)
    return s_hid.SendReport(0x01, &rep, sizeof(rep), 10);
}
