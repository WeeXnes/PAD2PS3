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
    uint8_t unk0;        // 0x00
    uint8_t buttons1;    // Select, L3, R3, Start, Up, Right, Down, Left
    uint8_t buttons2;    // L2, R2, L1, R1, Triangle, Circle, Cross, Square
    uint8_t ps_button;   // Bit 0: PS button
    uint8_t unk1;        // 0x00
    uint8_t left_x;      // Left stick X (0..255, 128=center)
    uint8_t left_y;      // Left stick Y (0..255, 128=center)
    uint8_t right_x;     // Right stick X (0..255, 128=center)
    uint8_t right_y;     // Right stick Y (0..255, 128=center)
    uint8_t unk2[4];     // Reserved
    // Pressure sensitive buttons (0..255)
    uint8_t press_up;
    uint8_t press_right;
    uint8_t press_down;
    uint8_t press_left;
    uint8_t press_l2;
    uint8_t press_r2;
    uint8_t press_l1;
    uint8_t press_r1;
    uint8_t press_triangle;
    uint8_t press_circle;
    uint8_t press_cross;
    uint8_t press_square;
    uint8_t unk3[4];     // Reserved (4 bytes, making charge at byte offset 30)
    uint8_t charge;      // Battery status (0xEE = 100% full/charging, offset 30)
    uint8_t connection;  // Power status (0x02 = USB connected, offset 31)
    uint8_t unk4[9];     // Reserved
    int16_t accel_x;     // Accelerometer X
    int16_t accel_y;     // Accelerometer Y
    int16_t accel_z;     // Accelerometer Z
    int16_t gyro_z;      // Gyro Z
};
#pragma pack(pop)

// Public API
bool usb_hid_init();
bool usb_hid_ready();
bool usb_hid_send(int8_t x, int8_t y, int8_t z, int8_t rz, int8_t rx, int8_t ry, uint8_t hat, uint32_t buttons);

#endif // USB_HID_H
