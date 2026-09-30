#include "ble_gamepad.h"
#include "controller_state.h"
#include <NimBLEDevice.h>

static NimBLEAddress s_targetAddress;
static std::string s_targetName = "";
static NimBLEClient* s_pClient = nullptr;
static bool s_doConnect = false;
static bool s_connected = false;
static bool s_isScanning = false;
static unsigned long s_lastScanStart = 0;
static uint8_t s_prevData[64] = {0};
static size_t s_prevLen = 0;

static void onScanEnded(NimBLEScanResults results) {
    s_isScanning = false;
}

/**
 * Maps the 10-byte Google Stadia BLE input report directly into the PS3 controller state.
 *
 * Byte 0: D-Pad Hat Switch (0=Up, 1=Up-Right, 2=Right, ..., 7=Up-Left, 8=Centered)
 * Byte 1: Left Trigger (L2) analog (0..255)
 * Byte 2: Right Trigger (R2) analog (0..255)
 * Byte 3: Left Stick X (0..255, 128=center)
 * Byte 4: Left Stick Y (0..255, 128=center)
 * Byte 5: Right Stick X (0..255, 128=center)
 * Byte 6: Right Stick Y (0..255, 128=center)
 * Byte 7: Extra analog / reserved
 * Byte 8: Action Buttons (A=0x01, B=0x02, X=0x04, Y=0x08, L1=0x10, R1=0x20, L3=0x40, R3=0x80)
 * Byte 9: System Buttons (Options=0x01, Menu=0x02, Stadia=0x04, Assistant=0x08, Capture=0x10)
 */
static void processStadiaReport(const uint8_t* pData, size_t length) {
    if (length < 10) return;

    // 1. D-Pad Hat Switch (Byte 0)
    uint8_t hat = pData[0];
    if (hat <= 7) {
        setDpad(hat + 1); // 0..7 -> 1..8 (DPAD_UP..DPAD_UP_LEFT)
    } else {
        setDpad(DPAD_CENTERED);
    }

    // 2. Analog Triggers L2 & R2 (Bytes 1 & 2)
    uint8_t l2 = pData[1];
    uint8_t r2 = pData[2];
    setAxis(AXIS_L2, l2);
    setAxis(AXIS_R2, r2);
    setButton(BTN_L2_DIGITAL, l2 > 30);
    setButton(BTN_R2_DIGITAL, r2 > 30);

    // 3. Analog Sticks (Bytes 3..6)
    setAxis(AXIS_LEFT_X,  pData[3]);
    setAxis(AXIS_LEFT_Y,  pData[4]);
    setAxis(AXIS_RIGHT_X, pData[5]);
    setAxis(AXIS_RIGHT_Y, pData[6]);

    // 4. Action Buttons & Bumpers & Stick Clicks (Byte 8)
    uint8_t b8 = pData[8];
    setButton(BTN_CROSS,    (b8 & (1 << 0)) != 0); // A -> Cross
    setButton(BTN_CIRCLE,   (b8 & (1 << 1)) != 0); // B -> Circle
    setButton(BTN_SQUARE,   (b8 & (1 << 2)) != 0); // X -> Square
    setButton(BTN_TRIANGLE, (b8 & (1 << 3)) != 0); // Y -> Triangle
    setButton(BTN_L1,       (b8 & (1 << 4)) != 0); // L1
    setButton(BTN_R1,       (b8 & (1 << 5)) != 0); // R1
    setButton(BTN_L3,       (b8 & (1 << 6)) != 0); // L3 (Left Click)
    setButton(BTN_R3,       (b8 & (1 << 7)) != 0); // R3 (Right Click)

    // 5. System Buttons (Byte 9)
    uint8_t b9 = pData[9];
    setButton(BTN_SELECT, (b9 & (1 << 0)) != 0); // Options (...) -> Select
    setButton(BTN_START,  (b9 & (1 << 1)) != 0); // Menu (hamburger) -> Start
    setButton(BTN_PS,     (b9 & (1 << 2)) != 0); // Stadia Logo -> PS Button
}

static void notifyCallback(NimBLERemoteCharacteristic* pBLERemoteCharacteristic,
                           uint8_t* pData, size_t length, bool isNotify) {
    if (length == 0 || pData == nullptr) return;

    // Filter out reports if data didn't change
    if (s_prevLen == length && memcmp(s_prevData, pData, length) == 0) {
        return;
    }

    // Map Stadia input directly into active PS3 Controller State
    processStadiaReport(pData, length);

    // Print live status
    Serial.printf("[STADIA -> PS3] DPAD:%u | LX:%3u LY:%3u | RX:%3u RY:%3u | L2:%3u R2:%3u | B8:0x%02X B9:0x%02X\n",
                  pData[0], pData[3], pData[4], pData[5], pData[6], pData[1], pData[2], pData[8], pData[9]);

    if (length <= sizeof(s_prevData)) {
        memcpy(s_prevData, pData, length);
        s_prevLen = length;
    }
}

class ClientCallbacks : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient* pClient) override {
        Serial.println("[BLE] Link layer connected!");
    }

    void onDisconnect(NimBLEClient* pClient) override {
        s_connected = false;
        s_prevLen = 0;
        resetControllerState(); // Release all inputs on disconnect
        Serial.println("[BLE] Disconnected from BLE gamepad!");
        Serial.println("[BLE] Resuming scan in 1 second...");
        s_lastScanStart = millis() + 1000;
    }

    bool onConnParamsUpdateRequest(NimBLEClient* pClient, const ble_gap_upd_params* params) override {
        // Accept and match peer's requested connection parameters so NimBLE doesn't clash
        pClient->setConnectionParams(params->itvl_min, params->itvl_max, params->latency, params->supervision_timeout);
        return true;
    }
};

static ClientCallbacks s_clientCallbacks;

class AdvertisedDeviceCallbacks : public NimBLEAdvertisedDeviceCallbacks {
    void onResult(NimBLEAdvertisedDevice* advertisedDevice) override {
        std::string name = advertisedDevice->getName();
        std::string addr = advertisedDevice->getAddress().toString();
        bool hasHidService = advertisedDevice->isAdvertisingService(NimBLEUUID((uint16_t)0x1812));

        bool isStadia = (name.find("Stadia") != std::string::npos);
        bool isGamepad = hasHidService || isStadia ||
                         (name.find("Gamepad") != std::string::npos) ||
                         (name.find("Controller") != std::string::npos) ||
                         (name.find("Wireless") != std::string::npos) ||
                         (name.find("Xbox") != std::string::npos) ||
                         (name.find("8BitDo") != std::string::npos);

        if (isGamepad) {
            Serial.println("==================================================");
            Serial.printf("[BLE SCAN] FOUND GAMEPAD!\n");
            Serial.printf("[BLE SCAN] Name: '%s'\n", name.c_str());
            Serial.printf("[BLE SCAN] MAC:  %s\n", addr.c_str());
            Serial.printf("[BLE SCAN] RSSI: %d dBm\n", advertisedDevice->getRSSI());
            Serial.println("==================================================");

            NimBLEDevice::getScan()->stop();
            s_isScanning = false;

            s_targetAddress = advertisedDevice->getAddress();
            s_targetName = name;
            s_doConnect = true;
        } else if (!name.empty()) {
            Serial.printf("[BLE SCAN] Nearby BLE: '%s' (%s)\n", name.c_str(), addr.c_str());
        }
    }
};

static AdvertisedDeviceCallbacks s_advCallbacks;

void ble_gamepad_init() {
    Serial.println("[BLE] Initializing NimBLE Bluetooth Stack...");
    NimBLEDevice::init("PS3-Bridge-Host");

    // Boost TX power for strong connection link
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);

    // Standard "Just Works" pairing with bonding enabled (essential for Google Stadia & BLE gamepads)
    NimBLEDevice::setSecurityAuth(true, false, true);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
    NimBLEDevice::setSecurityInitKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);
    NimBLEDevice::setSecurityRespKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);

    NimBLEScan* pScan = NimBLEDevice::getScan();
    pScan->setAdvertisedDeviceCallbacks(&s_advCallbacks, false);
    pScan->setActiveScan(true);
    pScan->setInterval(80);
    pScan->setWindow(60);

    ble_gamepad_start_scan();
}

void ble_gamepad_start_scan() {
    if (s_connected) {
        Serial.println("[BLE] Already connected to a gamepad. Disconnect first.");
        return;
    }

    NimBLEScan* pScan = NimBLEDevice::getScan();
    if (pScan->isScanning()) {
        pScan->stop();
    }

    Serial.println("[BLE] Starting BLE Gamepad Scan (looking for Stadia / BLE gamepads)...");
    s_isScanning = true;
    s_lastScanStart = millis();
    pScan->start(0, onScanEnded, false);
}

static bool connectToTarget(const NimBLEAddress& address, const std::string& name) {
    Serial.printf("[BLE] Connecting to %s (%s)...\n", name.c_str(), address.toString().c_str());

    // Small delay after stopping scan to let BLE radio settle
    delay(100);

    NimBLEClient* pClient = nullptr;
    if (NimBLEDevice::getClientListSize()) {
        pClient = NimBLEDevice::getClientByPeerAddress(address);
    }
    if (!pClient) {
        if (NimBLEDevice::getClientListSize() >= NIMBLE_MAX_CONNECTIONS) {
            pClient = NimBLEDevice::getDisconnectedClient();
        }
        if (!pClient) {
            pClient = NimBLEDevice::createClient();
        }
        pClient->setClientCallbacks(&s_clientCallbacks, false);
    }

    // Set connection parameters: 15ms interval, supervision timeout 600 (6000ms)
    pClient->setConnectionParams(12, 24, 0, 600);
    pClient->setConnectTimeout(10);

    if (!pClient->connect(address)) {
        Serial.println("[BLE] ERROR: Link connection failed! Retrying scan...");
        NimBLEDevice::deleteClient(pClient);
        s_connected = false;
        return false;
    }

    Serial.println("[BLE] Link established! Discovering HID service (0x1812)...");
    NimBLERemoteService* pHidService = pClient->getService(NimBLEUUID((uint16_t)0x1812));
    if (!pHidService) {
        Serial.println("[BLE] Performing full service discovery...");
        auto services = pClient->getServices(true);
        if (services) {
            for (auto s : *services) {
                if (s->getUUID() == NimBLEUUID((uint16_t)0x1812)) {
                    pHidService = s;
                    break;
                }
            }
        }
    }

    if (!pHidService) {
        Serial.println("[BLE] ERROR: No HID Service (0x1812) found on target device!");
        pClient->disconnect();
        return false;
    }

    Serial.println("[BLE] HID Service found! Discovering input report characteristics...");
    auto chars = pHidService->getCharacteristics(true);
    int subscribedCount = 0;

    if (chars) {
        for (auto chr : *chars) {
            if (chr->canNotify()) {
                Serial.printf("[BLE] Subscribing to HID Report Char %s (handle 0x%04X)...\n",
                              chr->getUUID().toString().c_str(), chr->getHandle());
                if (chr->subscribe(true, notifyCallback, false)) {
                    subscribedCount++;
                    Serial.println("[BLE] -> Subscribed successfully!");
                } else {
                    Serial.println("[BLE] -> Subscription failed!");
                }
            }
        }
    }

    s_connected = (subscribedCount > 0);
    if (s_connected) {
        s_pClient = pClient;
        s_prevLen = 0;
        Serial.println("==================================================");
        Serial.println("[BLE] *** STADIA CONTROLLER MAPPED DIRECTLY TO PS3! ***");
        Serial.printf("[BLE] Device: %s (%s)\n", name.c_str(), address.toString().c_str());
        Serial.println("[BLE] Wireless inputs are now actively driving PS3 DualShock 3!");
        Serial.println("==================================================");
    } else {
        Serial.println("[BLE] ERROR: No notifiable input reports could be subscribed.");
        pClient->disconnect();
    }

    return s_connected;
}

void ble_gamepad_loop() {
    if (s_doConnect) {
        s_doConnect = false;
        connectToTarget(s_targetAddress, s_targetName);
    }

    // Auto-restart scan if disconnected, not connecting, and timeout elapsed
    if (!s_connected && !s_doConnect) {
        NimBLEScan* pScan = NimBLEDevice::getScan();
        if (pScan && !pScan->isScanning() && millis() > s_lastScanStart) {
            ble_gamepad_start_scan();
        }
    }
}

bool ble_gamepad_is_connected() {
    return s_connected;
}

void ble_gamepad_print_status() {
    Serial.println("--- BLE Gamepad Status ---");
    Serial.printf("Connected: %s\n", s_connected ? "YES" : "NO");
    if (s_connected) {
        Serial.printf("Device:    %s (%s)\n", s_targetName.c_str(), s_targetAddress.toString().c_str());
    } else {
        NimBLEScan* pScan = NimBLEDevice::getScan();
        Serial.printf("Scanning:  %s\n", (pScan && pScan->isScanning()) ? "ACTIVE" : "IDLE");
    }
    Serial.println("--------------------------");
}
