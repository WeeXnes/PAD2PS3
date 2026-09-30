#ifndef USB_HID_H
#define USB_HID_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * Gamepad USB Identification
 * - Vendor ID: 0x054C (Sony Corp.)
 * - Product ID: 0x0268 (PlayStation 3 DualShock 3)
 * - Product Name: "DualShock 3 Compatible Controller"
 * - Manufacturer: "Sony Interactive Entertainment"
 */
#define USB_CONTROLLER_VID          0x054C
#define USB_CONTROLLER_PID          0x0268
#define USB_CONTROLLER_PRODUCT_NAME "PLAYSTATION(R)3 Controller"
#define USB_CONTROLLER_MANUFACTURER "Sony"

/**
 * Genuine Sony PS3 49-Byte Native Report Layout
 * (Architecture reserved for Phase 2 Native PS3 Console Pairing)
 */
#pragma pack(push, 1)
struct PS3NativeInputReport {
    uint8_t unk0;           // Byte 0: 0x00
    uint8_t buttons1;       // Byte 1: Select, L3, R3, Start, Up, Right, Down, Left
    uint8_t buttons2;       // Byte 2: L2, R2, L1, R1, Triangle, Circle, Cross, Square
    uint8_t ps_button;      // Byte 3: Bit 0 = PS button, other bits 0
    uint8_t unk1;           // Byte 4: 0x00
    uint8_t left_x;         // Byte 5: Left stick X (0..255, 128=center)
    uint8_t left_y;         // Byte 6: Left stick Y (0..255, 128=center)
    uint8_t right_x;        // Byte 7: Right stick X (0..255, 128=center)
    uint8_t right_y;        // Byte 8: Right stick Y (0..255, 128=center)
    uint8_t unk2[4];        // Bytes 9..12: Reserved (0x00)
    // Pressure sensitive buttons (0..255)
    uint8_t press_up;       // Byte 13
    uint8_t press_right;    // Byte 14
    uint8_t press_down;     // Byte 15
    uint8_t press_left;     // Byte 16
    uint8_t press_l2;       // Byte 17
    uint8_t press_r2;       // Byte 18
    uint8_t press_l1;       // Byte 19
    uint8_t press_r1;       // Byte 20
    uint8_t press_triangle; // Byte 21
    uint8_t press_circle;   // Byte 22
    uint8_t press_cross;    // Byte 23
    uint8_t press_square;   // Byte 24
    uint8_t unk3[3];        // Bytes 25..27: Reserved (0x00)
    uint8_t connection;     // Byte 28: 0x02 = USB connected
    uint8_t charge;         // Byte 29: 0xEE = 100% full/charging
    uint8_t status;         // Byte 30: 0x12 = PS3_WIRED_NO_RUMBLE
    uint8_t unk4[7];        // Bytes 31..37: Status bytes
    uint8_t accel_x_hi;     // Byte 38: Accel X MSB
    uint8_t accel_x_lo;     // Byte 39: Accel X LSB
    uint8_t accel_y_hi;     // Byte 40: Accel Y MSB
    uint8_t accel_y_lo;     // Byte 41: Accel Y LSB
    uint8_t accel_z_hi;     // Byte 42: Accel Z MSB
    uint8_t accel_z_lo;     // Byte 43: Accel Z LSB
    uint8_t gyro_z_hi;      // Byte 44: Gyro Z MSB
    uint8_t gyro_z_lo;      // Byte 45: Gyro Z LSB
    uint8_t reserved[2];    // Bytes 46..47: {0x00, 0x02}
};
#pragma pack(pop)

// Public API
bool usb_hid_init();
bool usb_hid_ready();
bool usb_hid_is_paired();
bool usb_hid_send(int8_t x, int8_t y, int8_t z, int8_t rz, int8_t rx, int8_t ry, uint8_t hat, uint32_t buttons);

#endif // USB_HID_H
