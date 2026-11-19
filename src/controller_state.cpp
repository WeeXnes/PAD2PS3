#include "controller_state.h"
#include "config_manager.h"
#include "usb_hid.h"
#include <Arduino.h>
#include <string.h>

static ControllerState s_rawState;
static ControllerState s_currentState;
static ControllerState s_lastReportedState;
static bool s_stateChanged = false;
static bool s_debugLog = false;
static portMUX_TYPE s_stateMux = portMUX_INITIALIZER_UNLOCKED;

static const char* BUTTON_NAMES[] = {
    "CROSS", "CIRCLE", "SQUARE", "TRIANGLE",
    "L1", "R1", "L2_BTN", "R2_BTN",
    "SELECT", "START", "L3", "R3",
    "PS", "EXTRA1", "EXTRA2", "EXTRA3"
};

void setDebugLog(bool enable) {
    s_debugLog = enable;
}

bool getDebugLog() {
    return s_debugLog;
}

static void initNeutralState(ControllerState& st) {
    st.buttons1 = 0;
    st.buttons2 = 0;
    st.dpad     = DPAD_CENTERED;
    st.leftX    = 128;
    st.leftY    = 128;
    st.rightX   = 128;
    st.rightY   = 128;
    st.L2       = 0;
    st.R2       = 0;
}

void resetControllerState() {
    portENTER_CRITICAL(&s_stateMux);
    initNeutralState(s_rawState);
    initNeutralState(s_currentState);
    s_stateChanged = true;
    portEXIT_CRITICAL(&s_stateMux);
}

void initControllerState() {
    portENTER_CRITICAL(&s_stateMux);
    initNeutralState(s_rawState);
    initNeutralState(s_currentState);
    s_lastReportedState = s_currentState;
    s_stateChanged = true;
    portEXIT_CRITICAL(&s_stateMux);
}

static void dpadToStick(uint8_t dpad, uint8_t& lx, uint8_t& ly) {
    switch (dpad) {
        case DPAD_UP:         lx = 128; ly = 0;   break;
        case DPAD_UP_RIGHT:   lx = 218; ly = 38;  break;
        case DPAD_RIGHT:      lx = 255; ly = 128; break;
        case DPAD_DOWN_RIGHT: lx = 218; ly = 218; break;
        case DPAD_DOWN:       lx = 128; ly = 255; break;
        case DPAD_DOWN_LEFT:  lx = 38;  ly = 218; break;
        case DPAD_LEFT:       lx = 0;   ly = 128; break;
        case DPAD_UP_LEFT:    lx = 38;  ly = 38;  break;
        default: break;
    }
}

void updateFromRawGamepadState(const ControllerState& raw) {
    ControllerConfig cfg = get_config_copy();
    ControllerState processed;
    initNeutralState(processed);

    // 1. Analog Sticks (handle optional Left/Right Stick swap + 2D radial deadzone & curves)
    uint8_t srcLX = cfg.swapSticks ? raw.rightX : raw.leftX;
    uint8_t srcLY = cfg.swapSticks ? raw.rightY : raw.leftY;
    uint8_t srcRX = cfg.swapSticks ? raw.leftX  : raw.rightX;
    uint8_t srcRY = cfg.swapSticks ? raw.leftY  : raw.rightY;

    apply_stick_curve_2d(srcLX, srcLY, cfg.leftStick, processed.leftX, processed.leftY);
    apply_stick_curve_2d(srcRX, srcRY, cfg.rightStick, processed.rightX, processed.rightY);

    // 2. D-Pad Mode (Standard, Left Stick emulation, or Both)
    if (cfg.dpadMode == DPAD_MODE_STANDARD) {
        processed.dpad = raw.dpad;
    } else if (cfg.dpadMode == DPAD_MODE_LEFT_STICK) {
        processed.dpad = DPAD_CENTERED;
        if (raw.dpad != DPAD_CENTERED) {
            dpadToStick(raw.dpad, processed.leftX, processed.leftY);
        }
    } else if (cfg.dpadMode == DPAD_MODE_BOTH) {
        processed.dpad = raw.dpad;
        if (raw.dpad != DPAD_CENTERED) {
            dpadToStick(raw.dpad, processed.leftX, processed.leftY);
        }
    }

    // 3. Analog Triggers L2 & R2 (handle optional swap + trigger response curves)
    uint8_t srcL2 = cfg.swapTriggers ? raw.R2 : raw.L2;
    uint8_t srcR2 = cfg.swapTriggers ? raw.L2 : raw.R2;

    processed.L2 = apply_trigger_curve(srcL2, cfg.triggerL2);
    processed.R2 = apply_trigger_curve(srcR2, cfg.triggerR2);

    // 4. Physical Buttons & Digital Trigger Thresholds -> Remapped PS3 Buttons
    uint16_t rawBits = (uint16_t)raw.buttons1 | ((uint16_t)raw.buttons2 << 8);

    // Evaluate digital L2/R2 bits using configurable digitalThreshold
    bool rawL2Click = ((rawBits & (1 << BTN_L2_DIGITAL)) != 0);
    bool rawR2Click = ((rawBits & (1 << BTN_R2_DIGITAL)) != 0);
    if (cfg.swapTriggers) {
        bool tmp = rawL2Click;
        rawL2Click = rawR2Click;
        rawR2Click = tmp;
    }
    bool l2DigitalActive = (srcL2 >= cfg.triggerL2.digitalThreshold) || (rawL2Click && srcL2 > cfg.triggerL2.deadzoneMin);
    bool r2DigitalActive = (srcR2 >= cfg.triggerR2.digitalThreshold) || (rawR2Click && srcR2 > cfg.triggerR2.deadzoneMin);

    if (l2DigitalActive) rawBits |= (1 << BTN_L2_DIGITAL);
    else                 rawBits &= ~(1 << BTN_L2_DIGITAL);

    if (r2DigitalActive) rawBits |= (1 << BTN_R2_DIGITAL);
    else                 rawBits &= ~(1 << BTN_R2_DIGITAL);

    uint16_t mappedBits = 0;
    for (uint8_t i = 0; i < NUM_MAPPED_BUTTONS; i++) {
        if (rawBits & (1 << i)) {
            uint8_t target = cfg.buttonMap[i];
            if (target < NUM_MAPPED_BUTTONS) {
                mappedBits |= (1 << target);
                // If a non-trigger button is mapped to L2 or R2, also saturate the analog trigger axis
                // so PS3 games that read analog pressure (press_l2/press_r2) register the press.
                if (target == BTN_L2_DIGITAL && i != BTN_L2_DIGITAL && processed.L2 < 255) {
                    processed.L2 = 255;
                }
                if (target == BTN_R2_DIGITAL && i != BTN_R2_DIGITAL && processed.R2 < 255) {
                    processed.R2 = 255;
                }
            }
        }
    }

    // If L2/R2 digital was remapped away to another button (e.g. L1/R1 tactical flip),
    // and nothing else mapped onto L2/R2, clear analog L2/R2 if appropriate or let swap handle it.
    if (cfg.buttonMap[BTN_L2_DIGITAL] != BTN_L2_DIGITAL && (mappedBits & (1 << BTN_L2_DIGITAL)) == 0) {
        processed.L2 = 0;
    }
    if (cfg.buttonMap[BTN_R2_DIGITAL] != BTN_R2_DIGITAL && (mappedBits & (1 << BTN_R2_DIGITAL)) == 0) {
        processed.R2 = 0;
    }

    processed.buttons1 = (uint8_t)(mappedBits & 0xFF);
    processed.buttons2 = (uint8_t)((mappedBits >> 8) & 0xFF);

    portENTER_CRITICAL(&s_stateMux);
    s_rawState = raw;
    if (memcmp(&s_currentState, &processed, sizeof(ControllerState)) != 0) {
        s_currentState = processed;
        s_stateChanged = true;
    }
    portEXIT_CRITICAL(&s_stateMux);
}

void reapplyActiveConfigToState() {
    ControllerState rawCopy = getRawControllerStateCopy();
    updateFromRawGamepadState(rawCopy);
}

void setButton(uint8_t button, bool pressed) {
    if (button >= 16) return;

    bool changed = false;
    portENTER_CRITICAL(&s_stateMux);
    if (button < 8) {
        uint8_t mask = (1 << button);
        bool currentlyPressed = (s_currentState.buttons1 & mask) != 0;
        if (currentlyPressed != pressed) {
            if (pressed) s_currentState.buttons1 |= mask;
            else         s_currentState.buttons1 &= ~mask;
            s_stateChanged = true;
            changed = true;
        }
    } else {
        uint8_t bit = button - 8;
        uint8_t mask = (1 << bit);
        bool currentlyPressed = (s_currentState.buttons2 & mask) != 0;
        if (currentlyPressed != pressed) {
            if (pressed) s_currentState.buttons2 |= mask;
            else         s_currentState.buttons2 &= ~mask;
            s_stateChanged = true;
            changed = true;
        }
    }
    portEXIT_CRITICAL(&s_stateMux);

    if (changed && s_debugLog) {
        Serial.printf("[CTRL] Button %s (idx %u) -> %s\n",
                      BUTTON_NAMES[button], button, pressed ? "PRESSED" : "RELEASED");
    }
}

void setAxis(uint8_t axis, uint8_t value) {
    portENTER_CRITICAL(&s_stateMux);
    uint8_t* target = nullptr;
    switch (axis) {
        case AXIS_LEFT_X:  target = &s_currentState.leftX;  break;
        case AXIS_LEFT_Y:  target = &s_currentState.leftY;  break;
        case AXIS_RIGHT_X: target = &s_currentState.rightX; break;
        case AXIS_RIGHT_Y: target = &s_currentState.rightY; break;
        case AXIS_L2:      target = &s_currentState.L2;     break;
        case AXIS_R2:      target = &s_currentState.R2;     break;
        default: break;
    }
    if (target && *target != value) {
        *target = value;
        s_stateChanged = true;
    }
    portEXIT_CRITICAL(&s_stateMux);
}

void setDpad(uint8_t dpad_val) {
    bool changed = false;
    portENTER_CRITICAL(&s_stateMux);
    if (s_currentState.dpad != dpad_val) {
        s_currentState.dpad = dpad_val;
        s_stateChanged = true;
        changed = true;
    }
    portEXIT_CRITICAL(&s_stateMux);

    if (changed && s_debugLog) {
        Serial.printf("[CTRL] D-Pad -> %u\n", dpad_val);
    }
}

static inline int8_t to_int8_axis(uint8_t val) {
    int res = (int)val - 128;
    if (res > 127) res = 127;
    if (res < -128) res = -128;
    return (int8_t)res;
}

void sendReport() {
    ControllerState snap;
    portENTER_CRITICAL(&s_stateMux);
    snap = s_currentState;
    s_lastReportedState = snap;
    s_stateChanged = false;
    portEXIT_CRITICAL(&s_stateMux);

    int8_t x  = to_int8_axis(snap.leftX);
    int8_t y  = to_int8_axis(snap.leftY);
    int8_t z  = to_int8_axis(snap.rightX);
    int8_t rz = to_int8_axis(snap.rightY);
    int8_t rx = to_int8_axis(snap.L2);
    int8_t ry = to_int8_axis(snap.R2);

    uint8_t hat = snap.dpad;
    uint32_t buttons = snap.buttons1 | ((uint32_t)snap.buttons2 << 8);

    usb_hid_send(x, y, z, rz, rx, ry, hat, buttons);
}

ControllerState getControllerStateCopy() {
    ControllerState copy;
    portENTER_CRITICAL(&s_stateMux);
    copy = s_currentState;
    portEXIT_CRITICAL(&s_stateMux);
    return copy;
}

ControllerState getRawControllerStateCopy() {
    ControllerState copy;
    portENTER_CRITICAL(&s_stateMux);
    copy = s_rawState;
    portEXIT_CRITICAL(&s_stateMux);
    return copy;
}

const ControllerState& getControllerState() {
    return s_currentState;
}

bool hasControllerStateChanged() {
    bool chg;
    portENTER_CRITICAL(&s_stateMux);
    chg = s_stateChanged;
    portEXIT_CRITICAL(&s_stateMux);
    return chg;
}

void clearStateChangedFlag() {
    portENTER_CRITICAL(&s_stateMux);
    s_stateChanged = false;
    portEXIT_CRITICAL(&s_stateMux);
}

void printControllerState() {
    ControllerState snap = getControllerStateCopy();
    char buf[128];
    snprintf(buf, sizeof(buf),
             "[CTRL STATE] B1:0x%02X B2:0x%02X DPAD:%u | LX:%3u LY:%3u | RX:%3u RY:%3u | L2:%3u R2:%3u\n",
             snap.buttons1, snap.buttons2, snap.dpad,
             snap.leftX, snap.leftY,
             snap.rightX, snap.rightY,
             snap.L2, snap.R2);
    Serial.print(buf);
}
