#include "ble_gamepad.h"
#include <NimBLEDevice.h>

static NimBLEAdvertisedDevice* s_targetDevice = nullptr;
static NimBLEClient* s_pClient = nullptr;
static bool s_doConnect = false;
static bool s_connected = false;
static bool s_isScanning = false;
static unsigned long s_lastScanStart = 0;
static uint8_t s_prevData[64] = {0};
static size_t s_prevLen = 0;
static std::string s_connectedName = "";
static std::string s_connectedAddress = "";

// Forward declarations
static bool connectToTarget(NimBLEAdvertisedDevice* target);

static void notifyCallback(NimBLERemoteCharacteristic* pBLERemoteCharacteristic,
                           uint8_t* pData, size_t length, bool isNotify) {
    if (length == 0 || pData == nullptr) return;

    // Filter out reports if data didn't change
    if (s_prevLen == length && memcmp(s_prevData, pData, length) == 0) {
        return;
    }

    Serial.printf("[BLE RAW] (%u bytes):", (unsigned)length);
    for (size_t i = 0; i < length; i++) {
        Serial.printf(" %02X", pData[i]);
    }
    Serial.println();

    // Print which bytes changed
    if (s_prevLen == length) {
        bool hasDiff = false;
        for (size_t i = 0; i < length; i++) {
            if (pData[i] != s_prevData[i]) {
                if (!hasDiff) {
                    Serial.print("   -> CHANGED: ");
                    hasDiff = true;
                } else {
                    Serial.print(", ");
                }
                uint8_t diff = pData[i] ^ s_prevData[i];
                Serial.printf("byte[%u]: 0x%02X -> 0x%02X (diff 0x%02X)", (unsigned)i, s_prevData[i], pData[i], diff);
            }
        }
        if (hasDiff) Serial.println();
    }

    if (length <= sizeof(s_prevData)) {
        memcpy(s_prevData, pData, length);
        s_prevLen = length;
    }
}

class ClientCallbacks : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient* pClient) override {
        Serial.println("[BLE] GATT connection established!");
    }

    void onDisconnect(NimBLEClient* pClient) override {
        s_connected = false;
        s_prevLen = 0;
        Serial.println("[BLE] Disconnected from BLE gamepad!");
        Serial.println("[BLE] Restarting scan in 2 seconds...");
        s_lastScanStart = millis() + 2000;
    }

    bool onConnParamsUpdateRequest(NimBLEClient* pClient, const ble_gap_upd_params* params) override {
        // Accept any connection parameter update requested by the gamepad
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

            if (s_targetDevice != nullptr) {
                delete s_targetDevice;
            }
            s_targetDevice = new NimBLEAdvertisedDevice(*advertisedDevice);
            s_doConnect = true;
        } else if (!name.empty()) {
            Serial.printf("[BLE SCAN] Nearby BLE device: '%s' (%s)\n", name.c_str(), addr.c_str());
        }
    }
};

static AdvertisedDeviceCallbacks s_advCallbacks;

void ble_gamepad_init() {
    Serial.println("[BLE] Initializing NimBLE Bluetooth Stack...");
    NimBLEDevice::init("PS3-Bridge-Host");

    // Enable bonding and encryption for HID gamepads like Google Stadia
    NimBLEDevice::setSecurityAuth(true, true, true);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
    NimBLEDevice::setSecurityInitKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);
    NimBLEDevice::setSecurityRespKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);

    NimBLEScan* pScan = NimBLEDevice::getScan();
    pScan->setAdvertisedDeviceCallbacks(&s_advCallbacks, false);
    pScan->setActiveScan(true);
    pScan->setInterval(100);
    pScan->setWindow(99);

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
    Serial.println("[BLE] TIP: For Google Stadia, hold 'Y' + Stadia button for 2s until orange light pulses!");
    s_isScanning = true;
    s_lastScanStart = millis();
    pScan->start(0, false); // 0 = continuous scan until found
}

static bool connectToTarget(NimBLEAdvertisedDevice* target) {
    if (!target) return false;

    s_connectedName = target->getName();
    s_connectedAddress = target->getAddress().toString();

    Serial.printf("[BLE] Connecting to %s (%s)...\n", s_connectedName.c_str(), s_connectedAddress.c_str());

    if (s_pClient == nullptr) {
        s_pClient = NimBLEDevice::createClient();
        s_pClient->setClientCallbacks(&s_clientCallbacks, false);
        // Standard fast BLE interval: 10-15ms for low latency controller input
        s_pClient->setConnectionParams(10, 15, 0, 150);
        s_pClient->setConnectTimeout(5);
    }

    if (!s_pClient->connect(target)) {
        Serial.println("[BLE] ERROR: Connection failed! Resuming scan...");
        s_connected = false;
        ble_gamepad_start_scan();
        return false;
    }

    Serial.println("[BLE] Connected! Securing link / bonding...");
    s_pClient->secureConnection();

    Serial.println("[BLE] Discovering HID service (0x1812)...");
    NimBLERemoteService* pHidService = s_pClient->getService(NimBLEUUID((uint16_t)0x1812));
    if (!pHidService) {
        Serial.println("[BLE] Warning: 0x1812 not found directly, performing full service discovery...");
        auto services = s_pClient->getServices(true);
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
        s_pClient->disconnect();
        return false;
    }

    Serial.println("[BLE] HID Service found! Discovering input report characteristics...");
    auto chars = pHidService->getCharacteristics(true);
    int subscribedCount = 0;

    if (chars) {
        for (auto chr : *chars) {
            // Report characteristic UUID is 0x2A4D
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

    if (subscribedCount == 0) {
        Serial.println("[BLE] Warning: Could not find notifiable 0x2A4D characteristic in 0x1812. Checking all services...");
        auto allServices = s_pClient->getServices(true);
        if (allServices) {
            for (auto s : *allServices) {
                auto sc = s->getCharacteristics(true);
                if (sc) {
                    for (auto c : *sc) {
                        if (c->canNotify() && c->getUUID() == NimBLEUUID((uint16_t)0x2A4D)) {
                            Serial.printf("[BLE] Found Report in service %s, subscribing...\n", s->getUUID().toString().c_str());
                            if (c->subscribe(true, notifyCallback, false)) {
                                subscribedCount++;
                            }
                        }
                    }
                }
            }
        }
    }

    s_connected = (subscribedCount > 0);
    if (s_connected) {
        s_prevLen = 0;
        Serial.println("==================================================");
        Serial.println("[BLE] *** BLE GAMEPAD SUCCESSFULLY PAIRED & CONNECTED! ***");
        Serial.printf("[BLE] Device: %s (%s)\n", s_connectedName.c_str(), s_connectedAddress.c_str());
        Serial.println("==================================================");
        Serial.println("[CALIBRATION WIZARD] Live button logger is ACTIVE.");
        Serial.println("Please press the controller buttons ONE AT A TIME in this order:");
        Serial.println("  1.  A / CROSS (Bottom action button)");
        Serial.println("  2.  B / CIRCLE (Right action button)");
        Serial.println("  3.  X / SQUARE (Left action button)");
        Serial.println("  4.  Y / TRIANGLE (Top action button)");
        Serial.println("  5.  D-PAD: UP");
        Serial.println("  6.  D-PAD: DOWN");
        Serial.println("  7.  D-PAD: LEFT");
        Serial.println("  8.  D-PAD: RIGHT");
        Serial.println("  9.  L1 / Left Shoulder Bumper");
        Serial.println(" 10.  R1 / Right Shoulder Bumper");
        Serial.println(" 11.  L2 / Left Trigger");
        Serial.println(" 12.  R2 / Right Trigger");
        Serial.println(" 13.  L3 / Left Stick Click");
        Serial.println(" 14.  R3 / Right Stick Click");
        Serial.println(" 15.  OPTIONS / SELECT (Left center button)");
        Serial.println(" 16.  MENU / START (Right center button)");
        Serial.println(" 17.  STADIA / PS Button (Middle center logo)");
        Serial.println(" 18.  ASSISTANT Button (if Stadia)");
        Serial.println(" 19.  CAPTURE Button (if Stadia)");
        Serial.println(" 20.  Left Stick full left/right and up/down");
        Serial.println(" 21.  Right Stick full left/right and up/down");
        Serial.println("==================================================");
    } else {
        Serial.println("[BLE] ERROR: No notifiable input reports could be subscribed.");
        s_pClient->disconnect();
    }

    return s_connected;
}

void ble_gamepad_loop() {
    if (s_doConnect && s_targetDevice != nullptr) {
        s_doConnect = false;
        connectToTarget(s_targetDevice);
        delete s_targetDevice;
        s_targetDevice = nullptr;
    }

    // Auto-restart scan if disconnected and not scanning
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
        Serial.printf("Device:    %s (%s)\n", s_connectedName.c_str(), s_connectedAddress.c_str());
    } else {
        NimBLEScan* pScan = NimBLEDevice::getScan();
        Serial.printf("Scanning:  %s\n", (pScan && pScan->isScanning()) ? "ACTIVE" : "IDLE");
    }
    Serial.println("--------------------------");
}
