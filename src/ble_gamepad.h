#pragma once

#include <Arduino.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * BLE Gamepad Client Manager for ESP32-S3
 * Connects to BLE HID gamepads (e.g., Google Stadia Controller, Xbox Wireless, 8BitDo in BLE mode)
 * and streams raw reports for input mapping.
 */

void ble_gamepad_init();
void ble_gamepad_loop();
bool ble_gamepad_is_connected();
void ble_gamepad_start_scan();
void ble_gamepad_print_status();
void ble_gamepad_set_rumble(uint8_t small_motor, uint8_t big_motor);
void ble_gamepad_set_rumble_intensity(uint8_t percent);
uint8_t ble_gamepad_get_rumble_intensity();
