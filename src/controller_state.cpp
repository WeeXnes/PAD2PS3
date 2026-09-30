#include "controller_state.h"
#include "usb_hid.h"
#include <Arduino.h>

static ControllerState s_currentState;
static ControllerState s_lastReportedState;
static bool s_stateChanged = false;

static const char* BUTTON_NAMES[] = {
    "CROSS", "CIRCLE", "SQUARE", "TRIANGLE",
    "L1", "R1", "L2_BTN", "R2_BTN",
    "SELECT", "START", "L3", "R3",
    "PS", "EXTRA1", "EXTRA2", "EXTRA3"
};

void resetControllerState() {
    s_currentState.buttons1 = 0;
    s_currentState.buttons2 = 0;
    s_currentState.dpad = DPAD_CENTERED; // 0 = Neutral / Centered in standard HID
    s_currentState.leftX = 128;          // Center
    s_currentState.leftY = 128;          // Center
    s_currentState.rightX = 128;         // Center
    s_currentState.rightY = 128;         // Center
    s_currentState.L2 = 0;               // Released
    s_currentState.R2 = 0;               // Released
    s_stateChanged = true;
}

void initControllerState() {
    resetControllerState();
    s_lastReportedState = s_currentState;
}

void setButton(uint8_t button, bool pressed) {
    if (button < 8) {
        uint8_t mask = (1 << button);
        bool currentlyPressed = (s_currentState.buttons1 & mask) != 0;
        if (currentlyPressed != pressed) {
            if (pressed) {
                s_currentState.buttons1 |= mask;
            } else {
                s_currentState.buttons1 &= ~mask;
            }
            s_stateChanged = true;
            Serial.printf("[CTRL] Button %s (idx %u) -> %s\n",
                          BUTTON_NAMES[button], button, pressed ? "PRESSED" : "RELEASED");
        }
    } else if (button < 16) {
        uint8_t bit = button - 8;
        uint8_t mask = (1 << bit);
        bool currentlyPressed = (s_currentState.buttons2 & mask) != 0;
        if (currentlyPressed != pressed) {
            if (pressed) {
                s_currentState.buttons2 |= mask;
            } else {
                s_currentState.buttons2 &= ~mask;
            }
            s_stateChanged = true;
            Serial.printf("[CTRL] Button %s (idx %u) -> %s\n",
                          BUTTON_NAMES[button], button, pressed ? "PRESSED" : "RELEASED");
        }
    }
}

void setAxis(uint8_t axis, uint8_t value) {
    uint8_t* target = nullptr;

    switch (axis) {
        case AXIS_LEFT_X:
            target = &s_currentState.leftX;
            break;
        case AXIS_LEFT_Y:
            target = &s_currentState.leftY;
            break;
        case AXIS_RIGHT_X:
            target = &s_currentState.rightX;
            break;
        case AXIS_RIGHT_Y:
            target = &s_currentState.rightY;
            break;
        case AXIS_L2:
            target = &s_currentState.L2;
            break;
        case AXIS_R2:
            target = &s_currentState.R2;
            break;
        default:
            return;
    }

    if (*target != value) {
        *target = value;
        s_stateChanged = true;
    }
}

void setDpad(uint8_t dpad_val) {
    if (s_currentState.dpad != dpad_val) {
        s_currentState.dpad = dpad_val;
        s_stateChanged = true;
        Serial.printf("[CTRL] D-Pad -> %u\n", dpad_val);
    }
}

static inline int8_t to_int8_axis(uint8_t val) {
    // Maps 0..255 -> -128..127
    int res = (int)val - 128;
    if (res > 127) res = 127;
    if (res < -128) res = -128;
    return (int8_t)res;
}

void sendReport() {
    int8_t x  = to_int8_axis(s_currentState.leftX);
    int8_t y  = to_int8_axis(s_currentState.leftY);
    int8_t z  = to_int8_axis(s_currentState.rightX);
    int8_t rz = to_int8_axis(s_currentState.rightY);
    int8_t rx = to_int8_axis(s_currentState.L2);
    int8_t ry = to_int8_axis(s_currentState.R2);

    uint8_t hat = s_currentState.dpad;
    uint32_t buttons = s_currentState.buttons1 | ((uint32_t)s_currentState.buttons2 << 8);

    usb_hid_send(x, y, z, rz, rx, ry, hat, buttons);
    s_lastReportedState = s_currentState;
    s_stateChanged = false;
}

const ControllerState& getControllerState() {
    return s_currentState;
}

bool hasControllerStateChanged() {
    return s_stateChanged;
}

void clearStateChangedFlag() {
    s_stateChanged = false;
}

void printControllerState() {
    char buf[128];
    snprintf(buf, sizeof(buf),
             "[CTRL STATE] B1:0x%02X B2:0x%02X DPAD:%u | LX:%3u LY:%3u | RX:%3u RY:%3u | L2:%3u R2:%3u\n",
             s_currentState.buttons1, s_currentState.buttons2, s_currentState.dpad,
             s_currentState.leftX, s_currentState.leftY,
             s_currentState.rightX, s_currentState.rightY,
             s_currentState.L2, s_currentState.R2);
    Serial.print(buf);
}
