#include <Arduino.h>
#include <math.h>
#include "usb_hid.h"
#include "controller_state.h"
#include "config_manager.h"
#include "ble_gamepad.h"
#include "web_server.h"

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

    // 2. Load persistent configuration (stick curves, trigger curves, button mappings, WiFi)
    config_init();

    // 3. Initialize Native USB HID stack (sets Sony VID 0x054C, PID 0x0268, and begins USB)
    usb_hid_init();

    // 4. Initialize BLE Central Gamepad Host (runs GATT/connect worker on Core 0)
    ble_gamepad_init();

    // 5. Initialize WiFi SoftAP Hotspot & Web UI Configuration Server (runs on Core 0)
    web_server_init();

    delay(200);

    logMsg("==================================================");
    logMsg("ESP32-S3 PS3 Controller Emulator (PAD2PS3 v2.0)");
    logMsg("Identity: Sony PLAYSTATION(R)3 Controller");
    logMsg("==================================================");
    logMsg("Waiting for USB host connection...");

    s_sequenceStartTime = millis();
    s_lastReportTime = millis();
}

/**
 * Executes the automated 10-second test sequence when 'demo' mode is toggled ON.
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

    if (currentPhase != s_lastActivePhase) {
        resetControllerState();
        s_lastActivePhase = currentPhase;
        Serial.printf("[MAIN] Transitioning to Test Phase %d\n", currentPhase);
    }

    if (currentPhase == 1) {
        if (elapsed < 285) {
            float t = (float)elapsed / 285.0f;
            setAxis(AXIS_LEFT_X, (uint8_t)(128.0f * (1.0f - t)));
            setAxis(AXIS_LEFT_Y, 128);
        } else if (elapsed < 570) {
            float t = (float)(elapsed - 285) / 285.0f;
            setAxis(AXIS_LEFT_X, (uint8_t)(255.0f * t));
            setAxis(AXIS_LEFT_Y, 128);
        } else if (elapsed < 855) {
            float t = (float)(elapsed - 570) / 285.0f;
            setAxis(AXIS_LEFT_X, (uint8_t)(255.0f - 127.0f * t));
            setAxis(AXIS_LEFT_Y, 128);
        } else if (elapsed < 1140) {
            float t = (float)(elapsed - 855) / 285.0f;
            setAxis(AXIS_LEFT_X, 128);
            setAxis(AXIS_LEFT_Y, (uint8_t)(128.0f * (1.0f - t)));
        } else if (elapsed < 1425) {
            float t = (float)(elapsed - 1140) / 285.0f;
            setAxis(AXIS_LEFT_X, 128);
            setAxis(AXIS_LEFT_Y, (uint8_t)(255.0f * t));
        } else if (elapsed < 1710) {
            float t = (float)(elapsed - 1425) / 285.0f;
            setAxis(AXIS_LEFT_X, 128);
            setAxis(AXIS_LEFT_Y, (uint8_t)(255.0f - 127.0f * t));
        } else {
            setAxis(AXIS_LEFT_X, 128);
            setAxis(AXIS_LEFT_Y, 128);
        }
    } else if (currentPhase == 2) {
        unsigned long buttonPhaseTime = elapsed - 2000;
        size_t activeIndex = buttonPhaseTime / 300;

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
        for (size_t i = 0; i < NUM_TEST_BUTTONS; i++) {
            setButton(TEST_BUTTONS[i], false);
        }
        setAxis(AXIS_LEFT_X, 128);
        setAxis(AXIS_LEFT_Y, 128);

        unsigned long phaseTime = elapsed - (2000 + (NUM_TEST_BUTTONS * 300));
        unsigned long phaseDuration = 10000 - (2000 + (NUM_TEST_BUTTONS * 300));

        float progress = (float)phaseTime / (float)phaseDuration;

        float angle = progress * 4.0f * (float)M_PI;
        uint8_t rx = (uint8_t)(128.0f + 120.0f * cosf(angle));
        uint8_t ry = (uint8_t)(128.0f + 120.0f * sinf(angle));
        setAxis(AXIS_RIGHT_X, rx);
        setAxis(AXIS_RIGHT_Y, ry);

        float triggerProgress = (progress < 0.5f) ? (progress * 2.0f) : ((1.0f - progress) * 2.0f);
        uint8_t triggerVal = (uint8_t)(triggerProgress * 255.0f);
        setAxis(AXIS_L2, triggerVal);
        setAxis(AXIS_R2, triggerVal);

        uint8_t dpadDir = 1 + ((uint8_t)(progress * 8.0f) % 8);
        setDpad(dpadDir);
    }
}

// ---------------------------------------------------------------------------
// Interactive Serial CLI State
// ---------------------------------------------------------------------------
static bool s_demoEnabled = false;
static unsigned long s_autoReleaseMs = 0;
static int8_t s_activeTapButton = -1;
static uint8_t s_activeTapDpad = 0;
static bool s_activeTapTriggerL2 = false;
static bool s_activeTapTriggerR2 = false;
static bool s_activeTapRumble = false;

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
    Serial.println("                    intensity <0-100> (set master rumble intensity percent)");
    Serial.println("                    rumble [0-255] (pulse controller vibration motors)");
    Serial.println("                    status, help / ?");
    Serial.println("  Bluetooth / BLE:  scan (start BLE scan for Stadia/gamepad)");
    Serial.println("                    ble (print BLE connection status)");
    Serial.println("  WiFi & Config:    wifi (print WiFi Hotspot & Web UI URL)");
    Serial.println("                    config (print active JSON configuration)");
    Serial.println("                    resetcfg (restore factory default settings)");
    Serial.println("==========================================================\n");
}

static void triggerButtonTap(uint8_t btn, const char* name, unsigned long currentMs, unsigned long durationMs = 150) {
    setButton(btn, true);
    s_activeTapButton = btn;
    s_autoReleaseMs = currentMs + durationMs;
    Serial.printf("[CLI] Tapped: %s (%lu ms)\n", name, durationMs);
}

static void triggerDpadTap(uint8_t dir, const char* name, unsigned long currentMs, unsigned long durationMs = 150) {
    setDpad(dir);
    s_activeTapDpad = dir;
    s_autoReleaseMs = currentMs + durationMs;
    Serial.printf("[CLI] Tapped D-Pad: %s\n", name);
}

static void triggerTriggerTap(uint8_t axis, uint8_t btn, const char* name, unsigned long currentMs, unsigned long durationMs = 150) {
    setAxis(axis, 255);
    setButton(btn, true);
    if (axis == AXIS_L2) s_activeTapTriggerL2 = true;
    else s_activeTapTriggerR2 = true;
    s_autoReleaseMs = currentMs + durationMs;
    Serial.printf("[CLI] Tapped: %s\n", name);
}

static void processSerialCLI(unsigned long currentMs) {
    // Check if Web UI requested a PS button tap/hold
    unsigned long webPulseMs = 0;
    if (web_server_consume_ps_pulse(webPulseMs)) {
        triggerButtonTap(BTN_PS, "PS (Web UI)", currentMs, webPulseMs);
    }

    // 1. Handle auto-release timers for tapped buttons / D-Pad
    if (s_autoReleaseMs > 0 && (long)(currentMs - s_autoReleaseMs) >= 0) {
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
        if (s_activeTapRumble) {
            ble_gamepad_set_rumble(0, 0, 0, 0);
            s_activeTapRumble = false;
        }
        s_autoReleaseMs = 0;
        if (ble_gamepad_is_connected()) {
            reapplyActiveConfigToState();
        }
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

            char* p = cmdBuf;
            while (*p && ((unsigned char)*p <= ' ' || (unsigned char)*p > 126)) p++;
            int len = strlen(p);
            while (len > 0 && ((unsigned char)p[len - 1] <= ' ' || (unsigned char)p[len - 1] > 126)) {
                p[--len] = '\0';
            }
            if (*p == '\0') continue;

            for (char* s = p; *s; s++) {
                if (*s >= 'A' && *s <= 'Z') *s += 32;
            }

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
                s_autoReleaseMs = 0;
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
            } else if (strncmp(p, "intensity", 9) == 0 || strncmp(p, "power", 5) == 0) {
                char* arg = p;
                while (*arg >= 'a' && *arg <= 'z') arg++;
                while (*arg == ' ') arg++;
                if (*arg) {
                    int val = atoi(arg);
                    if (val < 0) val = 0; else if (val > 100) val = 100;
                    ControllerConfig cfg = get_config_copy();
                    cfg.rumbleIntensity = (uint8_t)val;
                    set_config(cfg, true);
                    Serial.printf("[CLI] Rumble master intensity set to %d%% (saved)\n", val);
                } else {
                    Serial.printf("[CLI] Current rumble master intensity: %u%%\n", ble_gamepad_get_rumble_intensity());
                }
            } else if (strncmp(p, "rumble", 6) == 0) {
                int pwr = 255;
                if (strlen(p) > 6) {
                    char* arg = p + 6;
                    while (*arg == ' ') arg++;
                    if (*arg) {
                        pwr = atoi(arg);
                        if (pwr < 0) pwr = 0; else if (pwr > 255) pwr = 255;
                    }
                }
                ble_gamepad_set_rumble(pwr > 0 ? 1 : 0, (uint8_t)pwr, 500, 500);
                s_autoReleaseMs = currentMs + 500;
                s_activeTapRumble = true;
                Serial.printf("[CLI] Rumble pulse triggered at power %d (500ms)\n", pwr);
            } else if (strcmp(p, "scan") == 0) {
                ble_gamepad_disconnect_and_scan();
            } else if (strcmp(p, "ble") == 0) {
                ble_gamepad_print_status();
            } else if (strcmp(p, "wifi") == 0) {
                web_server_print_status();
            } else if (strcmp(p, "config") == 0) {
                char json[1024];
                config_to_json(json, sizeof(json));
                Serial.println(json);
            } else if (strcmp(p, "resetcfg") == 0) {
                ControllerConfig def;
                config_reset_defaults(def);
                set_config(def, true);
                reapplyActiveConfigToState();
                Serial.println("[CLI] Configuration reset to factory defaults and saved to NVS.");
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

    // 1. BLE Gamepad maintenance hook (non-blocking; heavy GATT runs on Core 0)
    ble_gamepad_loop();

    // 2. Process Interactive Serial CLI & Web UI actions
    processSerialCLI(currentMs);

    // 3. Check USB connection state & handle authentic PS3 pairing sequence
    static unsigned long s_connectedTime = 0;
    static unsigned long s_pulseStartTime = 0;
    static bool s_pairingPulseDone = false;
    static bool s_reportedPaired = false;
    static bool s_wasMounted = false;

    if (usb_hid_is_mounted()) {
        if (!s_wasMounted) {
            s_wasMounted = true;
            s_connectedTime = currentMs;
            s_pulseStartTime = 0;
            s_pairingPulseDone = false;
            s_reportedPaired = false;
            Serial.println("[MAIN] Host USB mounted & ready! Waiting for PS3 feature exchange...");
        }

        // If PS3 re-initiated feature 0xF7 handshake (e.g., on game launch or mode switch),
        // re-run the PS button pairing pulse sequence automatically.
        if (usb_hid_consume_rehandshake()) {
            s_connectedTime = currentMs;
            s_pulseStartTime = 0;
            s_pairingPulseDone = false;
            s_reportedPaired = false;
            Serial.println("[MAIN] PS3 requested re-handshake! Re-arming PS pairing pulse...");
        }

        if (usb_hid_is_paired() && !s_reportedPaired) {
            s_reportedPaired = true;
            Serial.println("[MAIN] Native PS3 Console Handshake confirmed active!");
        }

        if (!s_pairingPulseDone) {
            // Wait until PS3 completes Feature Reports (0xF7), or 2500ms fallback for PC
            bool readyForPulse = usb_hid_handshake_ready() || (currentMs - s_connectedTime > 2500);

            if (!readyForPulse) {
                delay(1);
                return;
            }

            if (s_pulseStartTime == 0) {
                s_pulseStartTime = currentMs;
                Serial.println("[MAIN] Feature exchange done! Sending PS button pulse to pair with PS3...");
            }

            // Hold PS button until PS3 sends F4 command (usb_hid_is_paired()) or for 1200ms max
            if (!usb_hid_is_paired() && (currentMs - s_pulseStartTime < 1200)) {
                setButton(BTN_PS, true);
            } else {
                setButton(BTN_PS, false);
                if (ble_gamepad_is_connected()) {
                    reapplyActiveConfigToState();
                }
                s_pairingPulseDone = true;
                s_sequenceStartTime = currentMs;
                Serial.println("[MAIN] Controller paired & active! Web UI & Serial CLI ready.");
            }
        } else {
            if (s_demoEnabled) {
                updateAutomatedTestSequence(currentMs);
            }
        }

        // Send HID Report at designated interval (100Hz = 10ms)
        if (currentMs - s_lastReportTime >= REPORT_INTERVAL_MS) {
            if (s_pairingPulseDone && usb_hid_ready()) {
                s_lastReportTime = currentMs;
                sendReport();
            }
        }

        if (s_demoEnabled) {
            static unsigned long s_lastDebugPrint = 0;
            if (currentMs - s_lastDebugPrint >= 500) {
                s_lastDebugPrint = currentMs;
                printControllerState();
            }
        }
    } else {
        if (s_wasMounted) {
            s_wasMounted = false;
            s_connectedTime = 0;
            s_pulseStartTime = 0;
            s_pairingPulseDone = false;
            s_reportedPaired = false;
            resetControllerState();
            Serial.println("[MAIN] Host USB disconnected / idle.");
        }
    }

    // Yield to FreeRTOS scheduler / USB background tasks
    delay(1);
}
