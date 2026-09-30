#include <Arduino.h>
#include <math.h>
#include "usb_hid.h"
#include "controller_state.h"

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

    delay(300);

    logMsg("==================================================");
    logMsg("ESP32-S3 PS3 Controller Emulator");
    logMsg("Identity: Sony PLAYSTATION(R)3 Controller");
    logMsg("==================================================");
    logMsg("USB HID initialized successfully");

    // Send initial centered/released report
    sendReport();
    printControllerState();

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

void loop() {
    unsigned long currentMs = millis();

    // Check USB connection state & handle authentic PS3 pairing sequence
    static unsigned long s_connectedTime = 0;
    static bool s_pairingPulseDone = false;
    static bool s_reportedPaired = false;

    if (usb_hid_ready()) {
        if (s_connectedTime == 0) {
            s_connectedTime = currentMs;
            s_pairingPulseDone = false;
            s_reportedPaired = false;
            Serial.println("[MAIN] Host USB connected! Waiting 1.2s for USB enumeration handshake...");
        }

        if (usb_hid_is_paired() && !s_reportedPaired) {
            s_reportedPaired = true;
            Serial.println("[MAIN] Native PS3 Console Handshake confirmed active!");
        }

        unsigned long timeSinceConnect = currentMs - s_connectedTime;

        if (!s_pairingPulseDone) {
            if (timeSinceConnect < 1200) {
                // Keep neutral and DO NOT send interrupt reports yet while host requests descriptors
                resetControllerState();
                delay(1);
                return;
            } else if (timeSinceConnect < 2400) {
                // Hold PS button firmly for 1.2s (1.2-2.4s) to pair with PS3 console
                resetControllerState();
                setButton(BTN_PS, true);
            } else {
                // Release PS button
                resetControllerState();
                setButton(BTN_PS, false);
                s_pairingPulseDone = true;
                s_sequenceStartTime = currentMs; // Synchronize test sequence start
                Serial.println("[MAIN] PS button pairing pulse finished! Starting automated test loop.");
            }
        } else {
            // Pairing pulse complete, run automated test loop
            updateAutomatedTestSequence(currentMs);
        }
    } else {
        s_connectedTime = 0;
        s_pairingPulseDone = false;
        s_reportedPaired = false;
    }

    // Send HID Report at designated interval (100Hz)
    if (currentMs - s_lastReportTime >= REPORT_INTERVAL_MS) {
        s_lastReportTime = currentMs;

        sendReport();

        // Print debug status every 500ms
        static unsigned long s_lastDebugPrint = 0;
        if (currentMs - s_lastDebugPrint >= 500) {
            s_lastDebugPrint = currentMs;
            printControllerState();
        }
    }

    // Yield to freeRTOS scheduler / USB background tasks
    delay(1);
}
