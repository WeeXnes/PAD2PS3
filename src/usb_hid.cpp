#include "usb_hid.h"
#include <Arduino.h>
#include "USB.h"
#include "USBHID.h"

// ---------------------------------------------------------------------------
// Official Sony PlayStation 3 DualShock 3 HID Report Descriptor
// Includes genuine PS3 report format (Report 0x01: 49 bytes input, 48 bytes output/feature)
// plus Feature Reports 0x02, 0xEE, 0xEF, 0xF1, 0xF2, 0xF4, 0xF5, 0xF7, 0xF8
// to satisfy both the Linux kernel hid-sony driver and physical PS3 console handshakes.
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
    0x85, 0xf1,        //     Report ID (241 = 0xF1)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x40,        //     Report Count (64 bytes)
    0x09, 0x01,        //     Usage (Pointer)
    0xb1, 0x02,        //     Feature (Data,Var,Abs)
    0xc0,              //   End Collection
    0xa1, 0x02,        //   Collection (Logical)
    0x85, 0xf2,        //     Report ID (242 = 0xF2)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x40,        //     Report Count (64 bytes)
    0x09, 0x01,        //     Usage (Pointer)
    0xb1, 0x02,        //     Feature (Data,Var,Abs)
    0xc0,              //   End Collection
    0xa1, 0x02,        //   Collection (Logical)
    0x85, 0xf4,        //     Report ID (244 = 0xF4)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x04,        //     Report Count (4 bytes)
    0x09, 0x01,        //     Usage (Pointer)
    0xb1, 0x02,        //     Feature (Data,Var,Abs)
    0xc0,              //   End Collection
    0xa1, 0x02,        //   Collection (Logical)
    0x85, 0xf5,        //     Report ID (245 = 0xF5)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x40,        //     Report Count (64 bytes)
    0x09, 0x01,        //     Usage (Pointer)
    0xb1, 0x02,        //     Feature (Data,Var,Abs)
    0xc0,              //   End Collection
    0xa1, 0x02,        //   Collection (Logical)
    0x85, 0xf7,        //     Report ID (247 = 0xF7)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x40,        //     Report Count (64 bytes)
    0x09, 0x01,        //     Usage (Pointer)
    0xb1, 0x02,        //     Feature (Data,Var,Abs)
    0xc0,              //   End Collection
    0xa1, 0x02,        //   Collection (Logical)
    0x85, 0xf8,        //     Report ID (248 = 0xF8)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x40,        //     Report Count (64 bytes)
    0x09, 0x01,        //     Usage (Pointer)
    0xb1, 0x02,        //     Feature (Data,Var,Abs)
    0xc0,              //   End Collection
    0xc0               // End Collection
};

static_assert(sizeof(PS3NativeInputReport) == 48, "PS3NativeInputReport must be exactly 48 bytes");

// ---------------------------------------------------------------------------
// Captured Authentic DualShock 3 Handshake Response Data
// Direct byte captures from a genuine PS3 console handshake session
// ---------------------------------------------------------------------------
static const uint8_t s_ds3_resp_0x01[64] = {
    0x00, 0x01, 0x04, 0x00, 0x08, 0x0C, 0x01, 0x02, 0x18, 0x18, 0x18, 0x18, 0x09, 0x0A, 0x10, 0x11,
    0x12, 0x13, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x02, 0x02, 0x02, 0x02, 0x00, 0x00, 0x00, 0x04,
    0x04, 0x04, 0x04, 0x00, 0x00, 0x04, 0x00, 0x01, 0x02, 0x07, 0x00, 0x17, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const uint8_t s_ds3_resp_0xF2[64] = {
    0xF2, 0xFF, 0xFF, 0x00, 0x34, 0xC7, 0x31, 0x28, 0xE8, 0x9E, 0x00, 0x03, 0x50, 0x81, 0xD8, 0x01,
    0x8A, 0x13, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x02, 0x02, 0x02, 0x02, 0x00, 0x00, 0x00, 0x04,
    0x04, 0x04, 0x04, 0x00, 0x00, 0x04, 0x00, 0x01, 0x02, 0x07, 0x00, 0x17, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const uint8_t s_ds3_resp_0xF5_template[64] = {
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xE8, 0x9E, 0x00, 0x03, 0x50, 0x81, 0xD8, 0x01,
    0x8A, 0x13, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x02, 0x02, 0x02, 0x02, 0x00, 0x00, 0x00, 0x04,
    0x04, 0x04, 0x04, 0x00, 0x00, 0x04, 0x00, 0x01, 0x02, 0x07, 0x00, 0x17, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const uint8_t s_ds3_resp_0xEF[64] = {
    0x00, 0xEF, 0x04, 0x00, 0x08, 0x03, 0x01, 0xA0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0xFE, 0x01, 0x8B, 0x02, 0x00, 0x01, 0x8F, 0x02, 0x00, 0x01, 0x8D, 0x01, 0xF4, 0x00,
    0x7D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const uint8_t s_ds3_resp_0xF8[64] = {
    0x00, 0x01, 0x00, 0x00, 0x08, 0x03, 0x01, 0xB0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x02, 0x6A, 0x02, 0x6E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const uint8_t s_ds3_resp_0xEF_b0[64] = {
    0x00, 0xEF, 0x04, 0x00, 0x08, 0x03, 0x01, 0xB0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x02, 0x6A, 0x02, 0x6E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const uint8_t s_ds3_resp_0xF7[64] = {
    0x01, 0x00, 0xFE, 0x02, 0xCE, 0x01, 0xEE, 0xFF, 0x10, 0x12, 0x00, 0x02, 0xFE, 0x01, 0xCE, 0x00,
    0x00, 0x02, 0x02, 0x02, 0x00, 0x03, 0x00, 0x00, 0x02, 0x00, 0x00, 0x02, 0x62, 0x01, 0x02, 0x01,
    0x5E, 0x00, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static uint8_t s_hostMac[6] = {0};
static bool s_hasHostMac = false;
static bool s_paired = false;
static bool s_streamingActive = false;
static uint8_t s_ef_subcmd = 0xA0;
static bool s_handshake_features_done = false;

class PS3USBHIDDevice : public USBHIDDevice {
public:
    PS3USBHIDDevice() {}

    uint16_t _onGetDescriptor(uint8_t* dst) override {
        memcpy(dst, s_sony_ds3_report_descriptor, sizeof(s_sony_ds3_report_descriptor));
        return sizeof(s_sony_ds3_report_descriptor);
    }

    uint16_t _onGetFeature(uint8_t report_id, uint8_t* buffer, uint16_t reqlen) override {
        Serial.printf("[USB HID] GET_REPORT (Feature): ID 0x%02X, req len %u\n", report_id, reqlen);

        const uint8_t* src = nullptr;

        switch (report_id) {
            case 0x01:
                src = s_ds3_resp_0x01;
                break;
            case 0xF2:
                src = s_ds3_resp_0xF2;
                break;
            case 0xF5: {
                static uint8_t f5_buf[64];
                memcpy(f5_buf, s_ds3_resp_0xF5_template, 64);
                if (s_hasHostMac) {
                    memcpy(&f5_buf[2], s_hostMac, 6);
                }
                src = f5_buf;
                break;
            }
            case 0xEF:
                if (s_ef_subcmd == 0xB0) {
                    src = s_ds3_resp_0xEF_b0;
                } else {
                    src = s_ds3_resp_0xEF;
                }
                break;
            case 0xF8:
                src = s_ds3_resp_0xF8;
                break;
            case 0xF7:
                src = s_ds3_resp_0xF7;
                s_handshake_features_done = true;
                Serial.println("[USB HID] *** Handshake features completed (0xF7)! Ready for PS button pulse ***");
                break;
            default:
                break;
        }

        if (src) {
            // Overwrite starting at buffer - 1 (p_epbuf->ctrl[0]) so byte 0 matches
            // authentic DS3 wire bytes (compensating for TinyUSB's default report_id injection)
            uint8_t* wire_buf = buffer - 1;
            memcpy(wire_buf, src, 64);
            return 63; // 1 + 63 = 64 bytes total sent by tud_control_xfer
        }

        if (reqlen > 0) {
            memset(buffer, 0, reqlen);
            return reqlen;
        }
        return 0;
    }

    void _onSetFeature(uint8_t report_id, const uint8_t* buffer, uint16_t len) override {
        Serial.printf("[USB HID] SET_REPORT (Feature): ID 0x%02X, len %u\n", report_id, len);

        if (report_id == 0xEF && len >= 7) {
            s_ef_subcmd = buffer[6]; // 0xA0 or 0xB0
            Serial.printf("[USB HID] SET_REPORT 0xEF subcmd: 0x%02X\n", s_ef_subcmd);
        } else if (report_id == 0xF4) {
            if (len >= 2) {
                Serial.printf("[USB HID] F4 Controller Command: 0x%02X 0x%02X\n", buffer[0], buffer[1]);
                if (buffer[0] == 0x42 && (buffer[1] == 0x02 || buffer[1] == 0x0C)) {
                    s_streamingActive = true;
                    s_paired = true;
                    Serial.println("[USB HID] *** PS3 ACTIVATED CONTROLLER (F4 Enable Streaming) ***");
                }
            }
        } else if (report_id == 0xF5) {
            if (len >= 8) {
                memcpy(s_hostMac, &buffer[2], 6);
                s_hasHostMac = true;
                Serial.printf("[USB HID] PS3 Paired Host MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                              s_hostMac[0], s_hostMac[1], s_hostMac[2],
                              s_hostMac[3], s_hostMac[4], s_hostMac[5]);
            }
        } else if (report_id == 0x01) {
            handleOutputReport(buffer, len);
        }
    }

    void _onOutput(uint8_t report_id, const uint8_t* buffer, uint16_t len) override {
        Serial.printf("[USB HID] SET_REPORT (Output): ID 0x%02X, len %u\n", report_id, len);
        if (report_id == 0x01 || report_id == 0x00) {
            handleOutputReport(buffer, len);
        }
    }

private:
    void handleOutputReport(const uint8_t* buffer, uint16_t len) {
        if (len >= 10) {
            uint8_t led_field = buffer[9];
            uint8_t leds = (led_field >> 1) & 0x0F;
            if (leds != 0) {
                s_paired = true;
                int player = (leds & 1) ? 1 : ((leds & 2) ? 2 : ((leds & 4) ? 3 : 4));
                Serial.printf("[USB HID] *** PS3 ASSIGNED PLAYER %d (LED field 0x%02X) ***\n", player, led_field);
            }
        }
    }
};

static USBHID s_hid;
static PS3USBHIDDevice s_device;
static bool s_initialized = false;

extern "C" uint8_t const * tud_hid_descriptor_report_cb(uint8_t instance);
extern USBHIDDevice * tinyusb_get_device_by_report_id(uint8_t report_id);

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
    USB.usbAttributes(0x80);              // bmAttributes = 0x80 (Bus Powered)
    USB.webUSB(false);                    // Disable WebUSB to prevent BOS descriptors

    if (!s_hid.addDevice(&s_device, sizeof(s_sony_ds3_report_descriptor))) {
        Serial.println("[USB HID] ERROR: addDevice failed!");
        return false;
    }

    s_hid.begin();
    bool usb_ok = USB.begin();

    // Critical PS3 Compatibility Fix:
    // PS3 console never requests GET_DESCRIPTOR HID_REPORT (0x22).
    // In Arduino ESP32 USBHID, device report IDs are ONLY loaded on demand when
    // tud_hid_descriptor_report_cb() is invoked. Because PS3 never queries it,
    // tud_hid_descriptor_report_cb() was never called, leaving report_ids empty
    // and causing TinyUSB to STALL all PS3 feature report queries!
    // We explicitly call tud_hid_descriptor_report_cb(0) here to parse and register
    // all report IDs immediately at boot.
    uint8_t const* desc = tud_hid_descriptor_report_cb(0);
    Serial.printf("[USB HID] Pre-loaded report map: %p\n", desc);

    const uint8_t check_ids[] = {0x01, 0x02, 0xEE, 0xEF, 0xF1, 0xF2, 0xF4, 0xF5, 0xF7, 0xF8};
    for (uint8_t id : check_ids) {
        USBHIDDevice* dev = tinyusb_get_device_by_report_id(id);
        Serial.printf("[USB HID] Report 0x%02X -> %s\n", id, dev ? "READY" : "MISSING!");
    }

    Serial.printf("[USB] VID: %d, PID: %d, Prod: %d, Mfr: %d, FW: %d, USB begin: %d\n",
                  vid_ok, pid_ok, prod_ok, mfr_ok, fw_ok, usb_ok);

    s_initialized = true;
    return true;
}

#include "tusb.h"

bool usb_hid_is_mounted() {
    bool m = s_initialized && tud_mounted();
    if (!m) {
        s_handshake_features_done = false;
        s_streamingActive = false;
        s_paired = false;
    }
    return m;
}

bool usb_hid_ready() {
    return usb_hid_is_mounted() && s_hid.ready();
}

bool usb_hid_handshake_ready() {
    return s_handshake_features_done;
}

bool usb_hid_is_paired() {
    return s_paired || s_streamingActive;
}

bool usb_hid_send(int8_t x, int8_t y, int8_t z, int8_t rz, int8_t rx, int8_t ry, uint8_t hat, uint32_t buttons) {
    if (!s_initialized || !((bool)USB)) {
        return false;
    }

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

    // Standard Authentic Status values matching captured PS3 packet #53
    rep.connection = 0x02; // USB
    rep.charge     = 0xEE; // 100% full / charging
    rep.status     = 0x12; // PS3_WIRED_NO_RUMBLE
    rep.unk4[4]    = 0x12;
    rep.unk4[5]    = 0xA2;
    rep.unk4[6]    = 0x77;

    // Authentic Sixaxis sensor center values
    rep.accel_x_hi = 0x01; rep.accel_x_lo = 0xC0; // 448
    rep.accel_y_hi = 0x01; rep.accel_y_lo = 0xFE; // 510
    rep.accel_z_hi = 0x01; rep.accel_z_lo = 0xEA; // 490
    rep.gyro_z_hi  = 0x01; rep.gyro_z_lo  = 0x8D; // 397
    rep.reserved[0] = 0x00;
    rep.reserved[1] = 0x02;

    // Send 48 bytes with Report ID 0x01 (TinyUSB prepends ID making 49 bytes on wire)
    return s_hid.SendReport(0x01, &rep, sizeof(rep), 25);
}

// ---------------------------------------------------------------------------
// Authentic PlayStation 3 DualShock 3 Configuration Descriptor (41 Bytes)
// Direct capture from genuine Sony controller:
// Interface 0: HID, SubClass 0, Protocol 0, iInterface 0, wDescriptorLength 148 (0x94)
// Endpoint 0x02 OUT (Interrupt, 64 bytes, 1ms)
// Endpoint 0x81 IN  (Interrupt, 64 bytes, 1ms)
// ---------------------------------------------------------------------------
static const uint8_t s_genuine_ds3_config_descriptor[] = {
    0x09,        // bLength
    0x02,        // bDescriptorType (Configuration)
    0x29, 0x00,  // wTotalLength: 41 bytes
    0x01,        // bNumInterfaces: 1
    0x01,        // bConfigurationValue: 1
    0x00,        // iConfiguration: 0
    0x80,        // bmAttributes: 0x80 (Bus Powered)
    0xFA,        // bMaxPower: 500mA

    0x09,        // bLength
    0x04,        // bDescriptorType (Interface)
    0x00,        // bInterfaceNumber: 0
    0x00,        // bAlternateSetting: 0
    0x02,        // bNumEndpoints: 2
    0x03,        // bInterfaceClass: 3 (HID)
    0x00,        // bInterfaceSubClass: 0
    0x00,        // bInterfaceProtocol: 0
    0x00,        // iInterface: 0 (No string)

    0x09,        // bLength
    0x21,        // bDescriptorType (HID)
    0x11, 0x01,  // bcdHID: 1.17
    0x00,        // bCountryCode: 0
    0x01,        // bNumDescriptors: 1
    0x22,        // bDescriptorType[0]: 0x22 (HID Report)
    0x94, 0x00,  // wDescriptorLength[0]: 148 bytes (0x0094)

    0x07,        // bLength
    0x05,        // bDescriptorType (Endpoint)
    0x02,        // bEndpointAddress: 0x02 (OUT)
    0x03,        // bmAttributes: 0x03 (Interrupt)
    0x40, 0x00,  // wMaxPacketSize: 64 bytes
    0x01,        // bInterval: 1ms

    0x07,        // bLength
    0x05,        // bDescriptorType (Endpoint)
    0x81,        // bEndpointAddress: 0x81 (IN)
    0x03,        // bmAttributes: 0x03 (Interrupt)
    0x40, 0x00,  // wMaxPacketSize: 64 bytes
    0x01         // bInterval: 1ms
};

// ---------------------------------------------------------------------------
// Authentic PlayStation 3 DualShock 3 Device Descriptor (18 Bytes)
// ---------------------------------------------------------------------------
static const uint8_t s_genuine_ds3_device_descriptor[] = {
    0x12,        // bLength: 18
    0x01,        // bDescriptorType: Device
    0x00, 0x02,  // bcdUSB: 2.00
    0x00,        // bDeviceClass: 0
    0x00,        // bDeviceSubClass: 0
    0x00,        // bDeviceProtocol: 0
    0x40,        // bMaxPacketSize0: 64
    0x4C, 0x05,  // idVendor: 0x054C (Sony)
    0x68, 0x02,  // idProduct: 0x0268 (DualShock 3)
    0x00, 0x01,  // bcdDevice: 1.00
    0x01,        // iManufacturer: 1 ("Sony")
    0x02,        // iProduct: 2 ("PLAYSTATION(R)3 Controller")
    0x00,        // iSerialNumber: 0
    0x01         // bNumConfigurations: 1
};

extern "C" uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
    (void)index;
    return s_genuine_ds3_config_descriptor;
}

extern "C" uint8_t const *tud_descriptor_device_cb(void) {
    return s_genuine_ds3_device_descriptor;
}

extern "C" uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;
    static uint16_t desc_str[64];
    const char* str = nullptr;

    if (index == 0) {
        desc_str[0] = (TUSB_DESC_STRING << 8) | 4;
        desc_str[1] = 0x0409; // English (US)
        return desc_str;
    } else if (index == 1) {
        str = "Sony";
    } else if (index == 2) {
        str = "PLAYSTATION(R)3 Controller";
    } else {
        return nullptr;
    }

    uint8_t len = strlen(str);
    if (len > 62) len = 62;
    desc_str[0] = (TUSB_DESC_STRING << 8) | (2 * len + 2);
    for (uint8_t i = 0; i < len; i++) {
        desc_str[1 + i] = str[i];
    }
    return desc_str;
}

