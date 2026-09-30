#include <Arduino.h>
#include <math.h>
#include "usb_hid.h"
#include "controller_state.h"
#include "ble_gamepad.h"

// Report interval: 10ms = 100Hz standard USB controller polling rate
static const unsigned long REPORT_INTERVAL_MS = 10;
static unsigned long s_lastReportTime = 0;
static unsigned long s_sequenceStartTime = 0;
static int s_lastActivePhase = -1;

void logMsg(const char* msg) {
    Serial.println(msg);
}

// Button test list (13 buttons)
static const uint8_t TEST_BUTTONS[] = {
    BTN_CROSS,
    BTN_CIRCLE,
    BTN_SQUARE,
    BTN_TRIANGLE,
    BTN_L1,
    BTN_R1,
    BTN_L2_DIGITAL,
    BTN_R2_DIGITAL,
    BTN_SELECT,
    BTN_START,
    BTN_L3,
    BTN_R3,
    BTN_PS
};
static const size_t NUM_TEST_BUTTONS = sizeof(TEST_BUTTONS) / sizeof(TEST_BUTTONS[0]);

void setup() {
    // Hardware UART on GPIO 43/44 (connected to the board's "UART" Type-C port)
    Serial.begin(115200);

    // 1. Initialize controller state to neutral
    initControllerState();

    // 2. Initialize Native USB HID stack (sets Sony VID 0x054C, PID 0x0268, and begins USB)
    usb_hid_init();

    // 3. Initialize BLE Central Gamepad Host (for wireless Stadia / BLE gamepads)
    ble_gamepad_init();

    delay(300);

    logMsg("==================================================");
    logMsg("ESP32-S3 PS3 Controller Emulator");
    logMsg("Identity: Sony PLAYSTATION(R)3 Controller");
    logMsg("==================================================");
    logMsg("Waiting for USB host connection...");

    s_sequenceStartTime = millis();
    s_lastReportTime = millis();
}

/**
 * Executes the automated test sequence requested:
 * Total period: 10,000 ms (10 seconds), repeating forever.
 *
 * 0 - 2000 ms (0-2s): Left Stick Test
 *   - center -> full left -> full right -> center -> full up -> full down -> center
 *
 * 2000 - 5900 ms (2-5s): Button Test
 *   - Press each of the 13 buttons for 300ms in sequence
 *
 * 5900 - 10000 ms (5-10s): Right Stick & Trigger Sweep
 *   - Move right stick in a smooth circular pattern
 *   - Sweep L2 and R2 triggers from 0 to 255
 */
void updateAutomatedTestSequence(unsigned long currentMs) {
    unsigned long elapsed = (currentMs - s_sequenceStartTime) % 10000;
    int currentPhase = 0;

    if (elapsed < 2000) {
        currentPhase = 1;
    } else if (elapsed < 2000 + (NUM_TEST_BUTTONS * 300)) {
        currentPhase = 2;
    } else {
        currentPhase = 3;
    }

    // Clean reset ONLY when transitioning across major phases
    if (currentPhase != s_lastActivePhase) {
        resetControllerState();
        s_lastActivePhase = currentPhase;
        Serial.printf("[MAIN] Transitioning to Test Phase %d\n", currentPhase);
    }

    if (currentPhase == 1) {
        // ==============================================================
        // PHASE 1 (0-2s): Move Left Stick Slowly
        // ==============================================================
        if (elapsed < 285) {
            // center (128) -> full left (0)
            float t = (float)elapsed / 285.0f;
            setAxis(AXIS_LEFT_X, (uint8_t)(128.0f * (1.0f - t)));
            setAxis(AXIS_LEFT_Y, 128);
        } else if (elapsed < 570) {
            // full left (0) -> full right (255)
            float t = (float)(elapsed - 285) / 285.0f;
            setAxis(AXIS_LEFT_X, (uint8_t)(255.0f * t));
            setAxis(AXIS_LEFT_Y, 128);
        } else if (elapsed < 855) {
            // full right (255) -> center (128)
            float t = (float)(elapsed - 570) / 285.0f;
            setAxis(AXIS_LEFT_X, (uint8_t)(255.0f - 127.0f * t));
            setAxis(AXIS_LEFT_Y, 128);
        } else if (elapsed < 1140) {
            // center (128) -> full up (0)
            float t = (float)(elapsed - 855) / 285.0f;
            setAxis(AXIS_LEFT_X, 128);
            setAxis(AXIS_LEFT_Y, (uint8_t)(128.0f * (1.0f - t)));
        } else if (elapsed < 1425) {
            // full up (0) -> full down (255)
            float t = (float)(elapsed - 1140) / 285.0f;
            setAxis(AXIS_LEFT_X, 128);
            setAxis(AXIS_LEFT_Y, (uint8_t)(255.0f * t));
        } else if (elapsed < 1710) {
            // full down (255) -> center (128)
            float t = (float)(elapsed - 1425) / 285.0f;
            setAxis(AXIS_LEFT_X, 128);
            setAxis(AXIS_LEFT_Y, (uint8_t)(255.0f - 127.0f * t));
        } else {
            // center (128, 128)
            setAxis(AXIS_LEFT_X, 128);
            setAxis(AXIS_LEFT_Y, 128);
        }
    } else if (currentPhase == 2) {
        // ==============================================================
        // PHASE 2 (2-5.9s): Press Each Button Individually for 300ms
        // ==============================================================
        unsigned long buttonPhaseTime = elapsed - 2000;
        size_t activeIndex = buttonPhaseTime / 300;

        // Keep sticks neutral during button test
        setAxis(AXIS_LEFT_X, 128);
        setAxis(AXIS_LEFT_Y, 128);
        setAxis(AXIS_RIGHT_X, 128);
        setAxis(AXIS_RIGHT_Y, 128);
        setAxis(AXIS_L2, 0);
        setAxis(AXIS_R2, 0);
        setDpad(DPAD_CENTERED);

        for (size_t i = 0; i < NUM_TEST_BUTTONS; i++) {
            setButton(TEST_BUTTONS[i], (i == activeIndex));
        }
    } else {
        // ==============================================================
        // PHASE 3 (5.9-10s): Circular Right Stick & Trigger Sweep
        // ==============================================================
        // Release all buttons
        for (size_t i = 0; i < NUM_TEST_BUTTONS; i++) {
            setButton(TEST_BUTTONS[i], false);
        }
        setAxis(AXIS_LEFT_X, 128);
        setAxis(AXIS_LEFT_Y, 128);

        unsigned long phaseTime = elapsed - (2000 + (NUM_TEST_BUTTONS * 300));
        unsigned long phaseDuration = 10000 - (2000 + (NUM_TEST_BUTTONS * 300)); // ~4100ms

        float progress = (float)phaseTime / (float)phaseDuration;

        // Circular right stick (2 full rotations)
        float angle = progress * 4.0f * (float)M_PI;
        uint8_t rx = (uint8_t)(128.0f + 120.0f * cosf(angle));
        uint8_t ry = (uint8_t)(128.0f + 120.0f * sinf(angle));
        setAxis(AXIS_RIGHT_X, rx);
        setAxis(AXIS_RIGHT_Y, ry);

        // Sweep triggers from 0 to 255 and back to 0 (triangle wave)
        float triggerProgress = (progress < 0.5f) ? (progress * 2.0f) : ((1.0f - progress) * 2.0f);
        uint8_t triggerVal = (uint8_t)(triggerProgress * 255.0f);
        setAxis(AXIS_L2, triggerVal);
        setAxis(AXIS_R2, triggerVal);

        // Cycle D-Pad hat switch smoothly (1..8)
        uint8_t dpadDir = 1 + ((uint8_t)(progress * 8.0f) % 8);
        setDpad(dpadDir);
    }
}

// ---------------------------------------------------------------------------
// Interactive Serial CLI State
// ---------------------------------------------------------------------------
static bool s_demoEnabled = false; // Automated demo disabled by default
static unsigned long s_autoReleaseMs = 0;
static int8_t s_activeTapButton = -1;
static uint8_t s_activeTapDpad = 0;
static bool s_activeTapTriggerL2 = false;
static bool s_activeTapTriggerR2 = false;

static void printCliHelp() {
    Serial.println("\n=================== PS3 CONTROLLER CLI ===================");
    Serial.println("  Action Buttons:   x (cross), o (circle), s (square), t (triangle)");
    Serial.println("  D-Pad:            u (up), d (down), l (left), r (right)");
    Serial.println("  Shoulders:        l1, r1, l2, r2, l3, r3");
    Serial.println("  System:           ps (tap), pshold (hold), psrel (release)");
    Serial.println("                    start, select");
    Serial.println("  Analog Sticks:    lx <0-255>   ly <0-255>");
    Serial.println("                    rx <0-255>   ry <0-255>");
    Serial.println("                    center / reset");
    Serial.println("  Modes & Utility:  demo (toggle automated demo loop on/off)");
    Serial.println("                    log / debug (toggle live input logging on/off)");
    Serial.println("                    status, help / ?");
    Serial.println("  Bluetooth / BLE:  scan (start BLE scan for Stadia/gamepad)");
    Serial.println("                    ble (print BLE connection status)");
    Serial.println("==========================================================\n");
}

static void triggerButtonTap(uint8_t btn, const char* name, unsigned long currentMs, unsigned long durationMs = 150) {
    resetControllerState();
    setButton(btn, true);
    s_activeTapButton = btn;
    s_autoReleaseMs = currentMs + durationMs;
    Serial.printf("[CLI] Tapped: %s\n", name);
}

static void triggerDpadTap(uint8_t dir, const char* name, unsigned long currentMs, unsigned long durationMs = 150) {
    resetControllerState();
    setDpad(dir);
    s_activeTapDpad = dir;
    s_autoReleaseMs = currentMs + durationMs;
    Serial.printf("[CLI] Tapped D-Pad: %s\n", name);
}

static void triggerTriggerTap(uint8_t axis, uint8_t btn, const char* name, unsigned long currentMs, unsigned long durationMs = 150) {
    resetControllerState();
    setAxis(axis, 255);
    setButton(btn, true);
    if (axis == AXIS_L2) s_activeTapTriggerL2 = true;
    else s_activeTapTriggerR2 = true;
    s_autoReleaseMs = currentMs + durationMs;
    Serial.printf("[CLI] Tapped: %s\n", name);
}

static void processSerialCLI(unsigned long currentMs) {
    // 1. Handle auto-release timers for tapped buttons / D-Pad
    if (s_autoReleaseMs > 0 && currentMs >= s_autoReleaseMs) {
        if (s_activeTapButton >= 0) {
            setButton(s_activeTapButton, false);
            s_activeTapButton = -1;
        }
        if (s_activeTapDpad != 0) {
            setDpad(DPAD_CENTERED);
            s_activeTapDpad = 0;
        }
        if (s_activeTapTriggerL2) {
            setAxis(AXIS_L2, 0);
            setButton(BTN_L2_DIGITAL, false);
            s_activeTapTriggerL2 = false;
        }
        if (s_activeTapTriggerR2) {
            setAxis(AXIS_R2, 0);
            setButton(BTN_R2_DIGITAL, false);
            s_activeTapTriggerR2 = false;
        }
        s_autoReleaseMs = 0;
    }

    // 2. Read incoming serial command lines
    static char cmdBuf[64];
    static size_t cmdLen = 0;

    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\r' || c == '\n') {
            if (cmdLen == 0) continue;
            cmdBuf[cmdLen] = '\0';
            cmdLen = 0;

            // Trim leading non-printable characters or whitespace
            char* p = cmdBuf;
            while (*p && ((unsigned char)*p <= ' ' || (unsigned char)*p > 126)) p++;
            // Trim trailing non-printable characters or whitespace
            int len = strlen(p);
            while (len > 0 && ((unsigned char)p[len - 1] <= ' ' || (unsigned char)p[len - 1] > 126)) {
                p[--len] = '\0';
            }
            if (*p == '\0') continue;

            // Convert to lowercase
            for (char* s = p; *s; s++) {
                if (*s >= 'A' && *s <= 'Z') *s += 32;
            }

            // Command dispatch
            if (strcmp(p, "help") == 0 || strcmp(p, "?") == 0) {
                printCliHelp();
            } else if (strcmp(p, "x") == 0 || strcmp(p, "cross") == 0 || strcmp(p, "k") == 0) {
                triggerButtonTap(BTN_CROSS, "CROSS", currentMs);
            } else if (strcmp(p, "o") == 0 || strcmp(p, "circle") == 0 || strcmp(p, "c") == 0 || strcmp(p, "j") == 0) {
                triggerButtonTap(BTN_CIRCLE, "CIRCLE", currentMs);
            } else if (strcmp(p, "s") == 0 || strcmp(p, "square") == 0 || strcmp(p, "sq") == 0 || strcmp(p, "h") == 0) {
                triggerButtonTap(BTN_SQUARE, "SQUARE", currentMs);
            } else if (strcmp(p, "t") == 0 || strcmp(p, "triangle") == 0 || strcmp(p, "tri") == 0 || strcmp(p, "y") == 0) {
                triggerButtonTap(BTN_TRIANGLE, "TRIANGLE", currentMs);
            } else if (strcmp(p, "u") == 0 || strcmp(p, "up") == 0 || strcmp(p, "w") == 0) {
                triggerDpadTap(DPAD_UP, "UP", currentMs);
            } else if (strcmp(p, "d") == 0 || strcmp(p, "down") == 0) {
                triggerDpadTap(DPAD_DOWN, "DOWN", currentMs);
            } else if (strcmp(p, "l") == 0 || strcmp(p, "left") == 0 || strcmp(p, "a") == 0) {
                triggerDpadTap(DPAD_LEFT, "LEFT", currentMs);
            } else if (strcmp(p, "r") == 0 || strcmp(p, "right") == 0) {
                triggerDpadTap(DPAD_RIGHT, "RIGHT", currentMs);
            } else if (strcmp(p, "l1") == 0) {
                triggerButtonTap(BTN_L1, "L1", currentMs);
            } else if (strcmp(p, "r1") == 0) {
                triggerButtonTap(BTN_R1, "R1", currentMs);
            } else if (strcmp(p, "l2") == 0) {
                triggerTriggerTap(AXIS_L2, BTN_L2_DIGITAL, "L2", currentMs);
            } else if (strcmp(p, "r2") == 0) {
                triggerTriggerTap(AXIS_R2, BTN_R2_DIGITAL, "R2", currentMs);
            } else if (strcmp(p, "l3") == 0) {
                triggerButtonTap(BTN_L3, "L3 (Stick Click)", currentMs);
            } else if (strcmp(p, "r3") == 0) {
                triggerButtonTap(BTN_R3, "R3 (Stick Click)", currentMs);
            } else if (strcmp(p, "start") == 0 || strcmp(p, "st") == 0) {
                triggerButtonTap(BTN_START, "START", currentMs);
            } else if (strcmp(p, "select") == 0 || strcmp(p, "sel") == 0) {
                triggerButtonTap(BTN_SELECT, "SELECT", currentMs);
            } else if (strcmp(p, "ps") == 0 || strcmp(p, "home") == 0) {
                triggerButtonTap(BTN_PS, "PS (pulse)", currentMs, 200);
            } else if (strcmp(p, "pshold") == 0) {
                setButton(BTN_PS, true);
                s_autoReleaseMs = 0; // Don't auto-release
                Serial.println("[CLI] Button PS HELD DOWN (type 'psrel' to release)");
            } else if (strcmp(p, "psrel") == 0) {
                setButton(BTN_PS, false);
                Serial.println("[CLI] Button PS RELEASED");
            } else if (strncmp(p, "lx ", 3) == 0) {
                int val = atoi(p + 3);
                if (val < 0) val = 0; else if (val > 255) val = 255;
                setAxis(AXIS_LEFT_X, (uint8_t)val);
                Serial.printf("[CLI] Left Stick X set to %d\n", val);
            } else if (strncmp(p, "ly ", 3) == 0) {
                int val = atoi(p + 3);
                if (val < 0) val = 0; else if (val > 255) val = 255;
                setAxis(AXIS_LEFT_Y, (uint8_t)val);
                Serial.printf("[CLI] Left Stick Y set to %d\n", val);
            } else if (strncmp(p, "rx ", 3) == 0) {
                int val = atoi(p + 3);
                if (val < 0) val = 0; else if (val > 255) val = 255;
                setAxis(AXIS_RIGHT_X, (uint8_t)val);
                Serial.printf("[CLI] Right Stick X set to %d\n", val);
            } else if (strncmp(p, "ry ", 3) == 0) {
                int val = atoi(p + 3);
                if (val < 0) val = 0; else if (val > 255) val = 255;
                setAxis(AXIS_RIGHT_Y, (uint8_t)val);
                Serial.printf("[CLI] Right Stick Y set to %d\n", val);
            } else if (strcmp(p, "center") == 0 || strcmp(p, "neutral") == 0 || strcmp(p, "reset") == 0) {
                resetControllerState();
                s_autoReleaseMs = 0;
                Serial.println("[CLI] All inputs reset to neutral");
            } else if (strcmp(p, "demo") == 0 || strcmp(p, "auto") == 0) {
                s_demoEnabled = !s_demoEnabled;
                resetControllerState();
                s_autoReleaseMs = 0;
                if (s_demoEnabled) {
                    s_sequenceStartTime = currentMs;
                    s_lastActivePhase = -1;
                    Serial.println("[CLI] Automated demo loop: ENABLED (type 'demo' to disable)");
                } else {
                    Serial.println("[CLI] Automated demo loop: DISABLED (controller in neutral standby)");
                }
            } else if (strcmp(p, "status") == 0) {
                printControllerState();
            } else if (strcmp(p, "log") == 0 || strcmp(p, "debug") == 0) {
                bool newState = !getDebugLog();
                setDebugLog(newState);
                Serial.printf("[CLI] Live input logging: %s (type 'log' to toggle)\n", newState ? "ENABLED" : "DISABLED");
            } else if (strcmp(p, "scan") == 0) {
                ble_gamepad_start_scan();
            } else if (strcmp(p, "ble") == 0) {
                ble_gamepad_print_status();
            } else {
                Serial.printf("[CLI] Unknown command: '%s'. Type 'help' for command list.\n", p);
            }
        } else if (cmdLen < sizeof(cmdBuf) - 1) {
            cmdBuf[cmdLen++] = c;
        }
    }
}

void loop() {
    unsigned long currentMs = millis();

    // 1. Process BLE Gamepad host scanning & incoming packet notifications
    ble_gamepad_loop();

    // 2. Process Interactive Serial CLI (always responsive)
    processSerialCLI(currentMs);

    // Check USB connection state & handle authentic PS3 pairing sequence
    static unsigned long s_connectedTime = 0;
    static unsigned long s_pulseStartTime = 0;
    static bool s_pairingPulseDone = false;
    static bool s_reportedPaired = false;
    static bool s_wasMounted = false;

    if (usb_hid_ready()) {
        if (!s_wasMounted) {
            s_wasMounted = true;
            s_connectedTime = currentMs;
            s_pulseStartTime = 0;
            s_pairingPulseDone = false;
            s_reportedPaired = false;
            Serial.println("[MAIN] Host USB mounted & ready! Waiting for PS3 feature exchange...");
        }

        if (usb_hid_is_paired() && !s_reportedPaired) {
            s_reportedPaired = true;
            Serial.println("[MAIN] Native PS3 Console Handshake confirmed active!");
        }

        if (!s_pairingPulseDone) {
            // DO NOT send ANY interrupt reports until PS3 completes Feature Reports (0xF7)!
            // Fallback timeout: 2500ms in case connected to PC where 0xF7 is not requested
            bool readyForPulse = usb_hid_handshake_ready() || (currentMs - s_connectedTime > 2500);

            if (!readyForPulse) {
                resetControllerState();
                delay(1);
                return;
            }

            if (s_pulseStartTime == 0) {
                s_pulseStartTime = currentMs;
                Serial.println("[MAIN] Feature exchange done! Sending PS button pulse to pair with PS3...");
            }

            // Hold PS button until PS3 sends F4 command (usb_hid_is_paired()) or for 1200ms max
            if (!usb_hid_is_paired() && (currentMs - s_pulseStartTime < 1200)) {
                resetControllerState();
                setButton(BTN_PS, true);
            } else {
                // Release PS button
                resetControllerState();
                setButton(BTN_PS, false);
                s_pairingPulseDone = true;
                s_sequenceStartTime = currentMs;
                Serial.println("[MAIN] Controller paired & active! Interactive Serial CLI ready.");
                Serial.println("[MAIN] Type 'help' or '?' for command list. Demo loop is DISABLED.");
            }
        } else {
            // Optional automated demo loop (only if toggled ON via 'demo' command)
            if (s_demoEnabled) {
                updateAutomatedTestSequence(currentMs);
            }
        }

        // Send HID Report at designated interval (100Hz = 10ms)
        if (currentMs - s_lastReportTime >= REPORT_INTERVAL_MS) {
            s_lastReportTime = currentMs;
            sendReport();

            // Only print periodic debug state if automated demo loop is enabled
            if (s_demoEnabled) {
                static unsigned long s_lastDebugPrint = 0;
                if (currentMs - s_lastDebugPrint >= 500) {
                    s_lastDebugPrint = currentMs;
                    printControllerState();
                }
            }
        }
    } else {
        if (s_wasMounted) {
            s_wasMounted = false;
            s_connectedTime = 0;
            s_pulseStartTime = 0;
            s_pairingPulseDone = false;
            s_reportedPaired = false;
            Serial.println("[MAIN] Host USB disconnected / idle.");
        }
        resetControllerState();
    }

    // Yield to freeRTOS scheduler / USB background tasks
    delay(1);
}
