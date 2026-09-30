#include "ble_gamepad.h"
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
        Serial.println("[BLE] Link layer connected!");
    }

    void onDisconnect(NimBLEClient* pClient) override {
        s_connected = false;
        s_prevLen = 0;
        Serial.println("[BLE] Disconnected from BLE gamepad!");
        Serial.println("[BLE] Resuming scan in 2 seconds...");
        s_lastScanStart = millis() + 2000;
    }

    bool onConnParamsUpdateRequest(NimBLEClient* pClient, const ble_gap_upd_params* params) override {
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
            Serial.printf("[BLE SCAN] Nearby BLE device: '%s' (%s)\n", name.c_str(), addr.c_str());
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
    Serial.println("[BLE] TIP: For Google Stadia, hold 'Y' + Stadia button for 2s until orange light pulses!");
    s_isScanning = true;
    s_lastScanStart = millis();
    // Non-blocking scan with callback
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

    // Set connection timeout to 10 seconds for secure bonding negotiation
    pClient->setConnectTimeout(10);

    if (!pClient->connect(address)) {
        Serial.println("[BLE] ERROR: Link connection failed! Retrying scan...");
        NimBLEDevice::deleteClient(pClient);
        s_connected = false;
        return false;
    }

    Serial.println("[BLE] Link established! Securing connection & bonding...");
    pClient->secureConnection();

    Serial.println("[BLE] Discovering HID service (0x1812)...");
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
        Serial.println("[BLE] *** BLE GAMEPAD SUCCESSFULLY PAIRED & CONNECTED! ***");
        Serial.printf("[BLE] Device: %s (%s)\n", name.c_str(), address.toString().c_str());
        Serial.println("==================================================");
        Serial.println("[CALIBRATION WIZARD] Live button logger is ACTIVE.");
        Serial.println("Please press the controller buttons ONE AT A TIME:");
        Serial.println("  1.  A / CROSS (Bottom action button)");
        Serial.println("  2.  B / CIRCLE (Right action button)");
        Serial.println("  3.  X / SQUARE (Left action button)");
        Serial.println("  4.  Y / TRIANGLE (Top action button)");
        Serial.println("  5.  D-PAD: UP, DOWN, LEFT, RIGHT");
        Serial.println("  6.  L1 & R1 (Shoulders)");
        Serial.println("  7.  L2 & R2 (Triggers)");
        Serial.println("  8.  L3 & R3 (Stick clicks)");
        Serial.println("  9.  OPTIONS / SELECT & MENU / START");
        Serial.println(" 10.  STADIA Logo Button");
        Serial.println(" 11.  ASSISTANT & CAPTURE buttons");
        Serial.println(" 12.  Left & Right Analog Sticks");
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
        Serial.printf("Device:    %s (%s)\n", s_targetName.c_str(), s_targetAddress.toString().c_str());
    } else {
        NimBLEScan* pScan = NimBLEDevice::getScan();
        Serial.printf("Scanning:  %s\n", (pScan && pScan->isScanning()) ? "ACTIVE" : "IDLE");
    }
    Serial.println("--------------------------");
}
