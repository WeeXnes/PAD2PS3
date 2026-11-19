#ifndef CONTROLLER_STATE_H
#define CONTROLLER_STATE_H

#include <stdint.h>
#include <stdbool.h>

/**
 * Controller Button Indices (0 to 15)
 * Matches PS3 DualShock button layout conventions
 */
enum ControllerButton : uint8_t {
    // buttons1 byte (Bits 0..7)
    BTN_CROSS       = 0,  // PlayStation Cross (A)
    BTN_CIRCLE      = 1,  // PlayStation Circle (B)
    BTN_SQUARE      = 2,  // PlayStation Square (X)
    BTN_TRIANGLE    = 3,  // PlayStation Triangle (Y)
    BTN_L1          = 4,  // Left Bumper
    BTN_R1          = 5,  // Right Bumper
    BTN_L2_DIGITAL  = 6,  // Digital L2 click
    BTN_R2_DIGITAL  = 7,  // Digital R2 click

    // buttons2 byte (Bits 0..7 -> indices 8..15)
    BTN_SELECT      = 8,  // Select / Back
    BTN_START       = 9,  // Start
    BTN_L3          = 10, // Left Thumbstick Click
    BTN_R3          = 11, // Right Thumbstick Click
    BTN_PS          = 12, // PS (Home / Guide) Button
    BTN_EXTRA1      = 13, // Stadia Assistant / Extra 1
    BTN_EXTRA2      = 14, // Stadia Capture / Extra 2
    BTN_EXTRA3      = 15  // Reserved / Extra 3
};

/**
 * D-Pad Hat Switch Directions
 * Standard USB HID Hat Switch:
 * 0 = Neutral / Centered
 * 1..8 = 8 clockwise directions starting at Up
 */
enum DPadDirection : uint8_t {
    DPAD_CENTERED   = 0,
    DPAD_UP         = 1,
    DPAD_UP_RIGHT   = 2,
    DPAD_RIGHT      = 3,
    DPAD_DOWN_RIGHT = 4,
    DPAD_DOWN       = 5,
    DPAD_DOWN_LEFT  = 6,
    DPAD_LEFT       = 7,
    DPAD_UP_LEFT    = 8
};

/**
 * Controller Analog Axes
 */
enum ControllerAxis : uint8_t {
    AXIS_LEFT_X     = 0,
    AXIS_LEFT_Y     = 1,
    AXIS_RIGHT_X    = 2,
    AXIS_RIGHT_Y    = 3,
    AXIS_L2         = 4,
    AXIS_R2         = 5
};

/**
 * Internal Controller State representation
 * Exactly matches the 9-byte logical state layout:
 * - buttons1: bits 0..7
 * - buttons2: bits 8..15
 * - dpad: hat switch 0=centered, 1..8=directions
 * - leftX, leftY: 0..255 (128 centered)
 * - rightX, rightY: 0..255 (128 centered)
 * - L2, R2: 0..255 (0 released, 255 full press)
 */
#pragma pack(push, 1)
struct ControllerState {
    uint8_t buttons1;
    uint8_t buttons2;

    uint8_t dpad;

    uint8_t leftX;
    uint8_t leftY;

    uint8_t rightX;
    uint8_t rightY;

    uint8_t L2;
    uint8_t R2;
};
#pragma pack(pop)

// Core state management API
void initControllerState();
void resetControllerState();

// Apply raw hardware gamepad state through active curves, deadzones, and button mappings
void updateFromRawGamepadState(const ControllerState& raw);
void reapplyActiveConfigToState();

// Direct control functions (CLI, pairing pulse, automated tests)
void setButton(uint8_t button, bool pressed);
void setAxis(uint8_t axis, uint8_t value);
void setDpad(uint8_t dpad_val);
void sendReport();

// Thread-safe state snapshots & debugging
ControllerState getControllerStateCopy();
ControllerState getRawControllerStateCopy();
const ControllerState& getControllerState();
bool hasControllerStateChanged();
void clearStateChangedFlag();
void printControllerState();
void setDebugLog(bool enable);
bool getDebugLog();

#endif // CONTROLLER_STATE_H
