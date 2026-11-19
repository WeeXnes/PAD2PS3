#include "ble_gamepad.h"
#include "controller_state.h"
#include "config_manager.h"
#include <NimBLEDevice.h>

static NimBLEAddress s_targetAddress;
static NimBLEAddress s_lastConnectedAddress;
static bool s_hasLastConnected = false;
static std::string s_targetName = "";
static NimBLEClient* s_pClient = nullptr;
static NimBLERemoteCharacteristic* s_pRumbleChar = nullptr;

static uint16_t s_lastRumbleLeft = 0;
static uint16_t s_lastRumbleRight = 0;
static volatile bool s_doConnect = false;
static volatile bool s_doDisconnect = false;
static volatile bool s_connected = false;
static volatile bool s_connecting = false;
static volatile bool s_isScanning = false;
static unsigned long s_lastScanStart = 0;

static uint8_t s_prevData[64] = {0};
static size_t s_prevLen = 0;
static unsigned long s_lastStateRefreshMs = 0;

static volatile uint32_t s_packetCount = 0;
static volatile unsigned long s_lastPacketMs = 0;
static volatile int s_lastRssi = -50;

static uint8_t s_targetRumbleSmall = 0;
static uint8_t s_targetRumbleBig = 0;
static unsigned long s_rumbleSmallExpireMs = 0;
static unsigned long s_rumbleBigExpireMs = 0;
static volatile bool s_rumbleChanged = false;
static unsigned long s_lastRumbleSendMs = 0;
static uint8_t s_rumbleIntensityPercent = 60;
static portMUX_TYPE s_rumbleMux = portMUX_INITIALIZER_UNLOCKED;

static TaskHandle_t s_bleWorkerTaskHandle = nullptr;

static void onScanEnded(NimBLEScanResults results) {
    s_isScanning = false;
}

/**
 * Maps the Google Stadia BLE input report into a Raw ControllerState and runs
 * it through the active deadzone, response curve, and button mapping pipeline.
 */
static void processStadiaReport(const uint8_t* pData, size_t length) {
    if (length < 7) return;

    ControllerState raw;
    memset(&raw, 0, sizeof(raw));

    // 1. D-Pad Hat Switch (Byte 0)
    uint8_t hat = pData[0] & 0x0F;
    if (hat <= 7) {
        raw.dpad = hat + 1; // 0..7 -> 1..8 (DPAD_UP..DPAD_UP_LEFT)
    } else {
        raw.dpad = DPAD_CENTERED;
    }

    // 2. Action Buttons & Bumpers & L3 (Byte 2)
    uint8_t b2 = pData[2];
    if (b2 & (1 << 6)) raw.buttons1 |= (1 << BTN_CROSS);    // A -> Cross
    if (b2 & (1 << 5)) raw.buttons1 |= (1 << BTN_CIRCLE);   // B -> Circle
    if (b2 & (1 << 4)) raw.buttons1 |= (1 << BTN_SQUARE);   // X -> Square
    if (b2 & (1 << 3)) raw.buttons1 |= (1 << BTN_TRIANGLE); // Y -> Triangle
    if (b2 & (1 << 2)) raw.buttons1 |= (1 << BTN_L1);       // L1 Bumper
    if (b2 & (1 << 1)) raw.buttons1 |= (1 << BTN_R1);       // R1 Bumper
    if (b2 & (1 << 0)) raw.buttons2 |= (1 << (BTN_L3 - 8)); // L3 (Left Stick Click)

    // 3. System Buttons & R3 & Clicks (Byte 1)
    uint8_t b1 = pData[1];
    if (b1 & (1 << 7)) raw.buttons2 |= (1 << (BTN_R3 - 8));     // R3 (Right Stick Click)
    if (b1 & (1 << 6)) raw.buttons2 |= (1 << (BTN_SELECT - 8)); // Options (...) -> Select
    if (b1 & (1 << 5)) raw.buttons2 |= (1 << (BTN_START - 8));  // Menu (hamburger) -> Start
    if (b1 & (1 << 4)) raw.buttons2 |= (1 << (BTN_PS - 8));     // Stadia Logo -> PS Button
    if (b1 & (1 << 3)) raw.buttons2 |= (1 << (BTN_EXTRA1 - 8)); // Assistant
    if (b1 & (1 << 2)) raw.buttons2 |= (1 << (BTN_EXTRA2 - 8)); // Capture
    if (b1 & (1 << 1)) raw.buttons1 |= (1 << BTN_L2_DIGITAL);   // Raw L2 digital bit
    if (b1 & (1 << 0)) raw.buttons1 |= (1 << BTN_R2_DIGITAL);   // Raw R2 digital bit

    // 4. Analog Sticks (Bytes 3..6)
    raw.leftX  = pData[3];
    raw.leftY  = pData[4];
    raw.rightX = pData[5];
    raw.rightY = pData[6];

    // 5. Analog Triggers L2 & R2 (Bytes 7 & 8)
    raw.L2 = (length > 7) ? pData[7] : 0;
    raw.R2 = (length > 8) ? pData[8] : 0;

    updateFromRawGamepadState(raw);
}

static void notifyCallback(NimBLERemoteCharacteristic* pBLERemoteCharacteristic,
                           uint8_t* pData, size_t length, bool isNotify) {
    if (length == 0 || pData == nullptr) return;

    unsigned long now = millis();
    s_packetCount++;
    s_lastPacketMs = now;

    bool dataIdentical = (s_prevLen == length && memcmp(s_prevData, pData, length) == 0);
    // Even if raw bytes haven't changed, refresh state every 200ms so held inputs
    // recover immediately after any CLI command or USB re-mount.
    if (dataIdentical && (now - s_lastStateRefreshMs < 200)) {
        return;
    }
    s_lastStateRefreshMs = now;

    processStadiaReport(pData, length);

    if (!dataIdentical && getDebugLog()) {
        uint8_t l2 = (length > 7) ? pData[7] : 0;
        uint8_t r2 = (length > 8) ? pData[8] : 0;
        uint8_t b1 = (length > 1) ? pData[1] : 0;
        uint8_t b2 = (length > 2) ? pData[2] : 0;

        Serial.printf("[STADIA -> PS3] DPAD:%u | LX:%3u LY:%3u | RX:%3u RY:%3u | L2:%3u R2:%3u | ACT:0x%02X SYS:0x%02X\n",
                      pData[0] & 0x0F, pData[3], pData[4], pData[5], pData[6], l2, r2, b2, b1);
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
        s_pRumbleChar = nullptr;
        s_lastRumbleLeft = 0;
        s_lastRumbleRight = 0;
        resetControllerState();
        Serial.println("[BLE] Disconnected from BLE gamepad!");
        Serial.println("[BLE] Resuming scan in 500ms...");
        s_lastScanStart = millis() + 500;
    }

    bool onConnParamsUpdateRequest(NimBLEClient* pClient, const ble_gap_upd_params* params) override {
        // Keep connection interval responsive and supervision timeout generous (>= 5 seconds)
        // so the gamepad does not drop after 30-40 minutes due to brief RF interference or high slave latency.
        uint16_t itvl_min = params->itvl_min < 6 ? 6 : params->itvl_min;
        uint16_t itvl_max = params->itvl_max > 28 ? 28 : params->itvl_max;
        if (itvl_max < itvl_min) itvl_max = itvl_min;
        uint16_t latency = params->latency > 2 ? 2 : params->latency;
        uint16_t timeout = params->supervision_timeout < 500 ? 600 : params->supervision_timeout;
        pClient->setConnectionParams(itvl_min, itvl_max, latency, timeout);
        return true;
    }
};

static ClientCallbacks s_clientCallbacks;

class AdvertisedDeviceCallbacks : public NimBLEAdvertisedDeviceCallbacks {
    void onResult(NimBLEAdvertisedDevice* advertisedDevice) override {
        if (s_connected || s_doConnect || s_connecting) return;

        std::string name = advertisedDevice->getName();
        NimBLEAddress devAddr = advertisedDevice->getAddress();
        std::string addr = devAddr.toString();
        bool hasHidService = advertisedDevice->isAdvertisingService(NimBLEUUID((uint16_t)0x1812));
        uint16_t appearance = advertisedDevice->getAppearance();
        bool isHidAppearance = (appearance == 0x03C4 || appearance == 0x03C3 || appearance == 0x03C0);
        bool isKnownReconnect = (s_hasLastConnected && devAddr == s_lastConnectedAddress);

        bool isStadia = (name.find("Stadia") != std::string::npos);
        bool isGamepad = isKnownReconnect || hasHidService || isHidAppearance || isStadia ||
                         (name.find("Gamepad") != std::string::npos) ||
                         (name.find("Controller") != std::string::npos) ||
                         (name.find("Wireless") != std::string::npos) ||
                         (name.find("Xbox") != std::string::npos) ||
                         (name.find("8BitDo") != std::string::npos) ||
                         (name.find("DualSense") != std::string::npos) ||
                         (name.find("DualShock") != std::string::npos);

        if (isGamepad) {
            Serial.println("==================================================");
            Serial.printf("[BLE SCAN] FOUND GAMEPAD!\n");
            Serial.printf("[BLE SCAN] Name: '%s'%s\n", name.c_str(), isKnownReconnect ? " (Fast Reconnect)" : "");
            Serial.printf("[BLE SCAN] MAC:  %s\n", addr.c_str());
            Serial.printf("[BLE SCAN] RSSI: %d dBm\n", advertisedDevice->getRSSI());
            Serial.println("==================================================");

            s_lastRssi = advertisedDevice->getRSSI();
            NimBLEDevice::getScan()->stop();
            s_isScanning = false;

            s_targetAddress = devAddr;
            if (!name.empty()) {
                s_targetName = name;
            } else if (s_targetName.empty()) {
                s_targetName = "BLE Gamepad";
            }
            s_doConnect = true;
        }
    }
};

static AdvertisedDeviceCallbacks s_advCallbacks;

static bool connectToTarget(const NimBLEAddress& address, const std::string& name) {
    s_connecting = true;
    Serial.printf("[BLE] Connecting to %s (%s)...\n", name.c_str(), address.toString().c_str());

    NimBLEScan* pScan = NimBLEDevice::getScan();
    if (pScan && pScan->isScanning()) {
        pScan->stop();
    }
    vTaskDelay(pdMS_TO_TICKS(80));

    // Clean up any stale client instances before creating a fresh connection
    // to prevent stale GATT attribute handle caches across reconnects.
    if (s_pClient) {
        if (s_pClient->isConnected()) {
            s_pClient->disconnect();
        }
        NimBLEClient* old = s_pClient;
        s_pClient = nullptr;
        s_pRumbleChar = nullptr;
        NimBLEDevice::deleteClient(old);
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    NimBLEClient* pClient = NimBLEDevice::getClientByPeerAddress(address);
    if (pClient) {
        NimBLEDevice::deleteClient(pClient);
        pClient = nullptr;
    }

    pClient = NimBLEDevice::createClient();
    if (!pClient) {
        Serial.println("[BLE] ERROR: Failed to allocate NimBLEClient!");
        s_connecting = false;
        return false;
    }

    pClient->setClientCallbacks(&s_clientCallbacks, false);
    // 15ms interval, 0 latency, 6000ms supervision timeout
    pClient->setConnectionParams(12, 24, 0, 600);
    pClient->setConnectTimeout(8);

    if (!pClient->connect(address)) {
        Serial.println("[BLE] ERROR: Link connection failed! Retrying scan...");
        NimBLEDevice::deleteClient(pClient);
        s_connected = false;
        s_connecting = false;
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
        NimBLEDevice::deleteClient(pClient);
        s_connecting = false;
        return false;
    }

    Serial.println("[BLE] HID Service found! Discovering input & output characteristics...");
    auto chars = pHidService->getCharacteristics(true);
    int subscribedCount = 0;
    s_pRumbleChar = nullptr;

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

            if (chr->canWrite() || chr->canWriteNoResponse()) {
                NimBLERemoteDescriptor* pDesc = chr->getDescriptor(NimBLEUUID((uint16_t)0x2908));
                if (pDesc) {
                    std::string descVal = pDesc->readValue();
                    if (descVal.length() >= 2) {
                        uint8_t repId = (uint8_t)descVal[0];
                        uint8_t repType = (uint8_t)descVal[1];
                        Serial.printf("[BLE] Writable Char %s (handle 0x%04X) -> Report ID 0x%02X, Type 0x%02X\n",
                                      chr->getUUID().toString().c_str(), chr->getHandle(), repId, repType);
                        if (repId == 0x05) {
                            s_pRumbleChar = chr;
                            Serial.println("[BLE] -> MATCHED Stadia Haptic Output (Report ID 5)!");
                        }
                    }
                } else if (!s_pRumbleChar) {
                    s_pRumbleChar = chr;
                }
            }
        }
    }

    if (subscribedCount > 0) {
        s_pClient = pClient;
        s_lastConnectedAddress = address;
        s_hasLastConnected = true;
        s_prevLen = 0;
        s_lastRumbleLeft = 0;
        s_lastRumbleRight = 0;
        s_lastPacketMs = millis();
        s_connected = true;
        Serial.println("==================================================");
        Serial.println("[BLE] *** GAMEPAD MAPPED DIRECTLY TO PS3! ***");
        Serial.printf("[BLE] Device: %s (%s)\n", name.c_str(), address.toString().c_str());
        Serial.printf("[BLE] Haptic/Rumble Feedback: %s\n", s_pRumbleChar ? "SUPPORTED (Active)" : "NOT FOUND");
        Serial.println("==================================================");
    } else {
        Serial.println("[BLE] ERROR: No notifiable input reports could be subscribed.");
        pClient->disconnect();
        NimBLEDevice::deleteClient(pClient);
        s_connected = false;
    }

    s_connecting = false;
    return s_connected;
}

void ble_gamepad_set_rumble_intensity(uint8_t percent) {
    if (percent > 100) percent = 100;
    portENTER_CRITICAL(&s_rumbleMux);
    s_rumbleIntensityPercent = percent;
    s_rumbleChanged = true;
    portEXIT_CRITICAL(&s_rumbleMux);
}

uint8_t ble_gamepad_get_rumble_intensity() {
    return s_rumbleIntensityPercent;
}

void ble_gamepad_set_rumble(uint8_t small_motor, uint8_t big_motor, uint16_t small_dur_ms, uint16_t big_dur_ms) {
    unsigned long now = millis();
    portENTER_CRITICAL(&s_rumbleMux);
    if (small_motor > 0 && small_dur_ms > 0) {
        s_rumbleSmallExpireMs = now + small_dur_ms;
    } else {
        small_motor = 0;
        s_rumbleSmallExpireMs = 0;
    }

    if (big_motor > 0 && big_dur_ms > 0) {
        s_rumbleBigExpireMs = now + big_dur_ms;
    } else {
        big_motor = 0;
        s_rumbleBigExpireMs = 0;
    }

    if (s_targetRumbleSmall != small_motor || s_targetRumbleBig != big_motor) {
        s_targetRumbleSmall = small_motor;
        s_targetRumbleBig = big_motor;
        s_rumbleChanged = true;
    }
    portEXIT_CRITICAL(&s_rumbleMux);
}

static void processPendingRumble() {
    if (!s_connected || s_pRumbleChar == nullptr || s_pClient == nullptr) {
        return;
    }
    if (!s_pClient->isConnected()) {
        return;
    }

    unsigned long now = millis();
    uint8_t smallVal = 0;
    uint8_t bigVal = 0;
    uint8_t masterPct = 60;
    bool shouldSend = false;

    portENTER_CRITICAL(&s_rumbleMux);
    // Auto-expire motors if PS3 stopped refreshing rumble duration
    if (s_targetRumbleSmall > 0 && s_rumbleSmallExpireMs > 0 && (long)(now - s_rumbleSmallExpireMs) >= 0) {
        s_targetRumbleSmall = 0;
        s_rumbleSmallExpireMs = 0;
        s_rumbleChanged = true;
    }
    if (s_targetRumbleBig > 0 && s_rumbleBigExpireMs > 0 && (long)(now - s_rumbleBigExpireMs) >= 0) {
        s_targetRumbleBig = 0;
        s_rumbleBigExpireMs = 0;
        s_rumbleChanged = true;
    }

    smallVal  = s_targetRumbleSmall;
    bigVal    = s_targetRumbleBig;
    masterPct = s_rumbleIntensityPercent;

    bool stopping = (smallVal == 0 && bigVal == 0);
    if (s_rumbleChanged && (stopping || (now - s_lastRumbleSendMs >= 50))) {
        s_rumbleChanged = false;
        s_lastRumbleSendMs = now;
        shouldSend = true;
    }
    portEXIT_CRITICAL(&s_rumbleMux);

    if (!shouldSend) return;

    ControllerConfig cfg = get_config_copy();

    // Right motor (small_motor): High-frequency light buzz on PS3
    uint32_t r_base = 0;
    if (smallVal == 1) {
        r_base = 24000;
    } else if (smallVal > 1) {
        r_base = ((uint32_t)smallVal * 24000) / 255;
    }
    r_base = (r_base * cfg.rumbleRightScale) / 100;

    // Left motor (big_motor): Heavy low-frequency rumble (0..255)
    uint32_t l_base = (uint32_t)bigVal * 257;
    if (l_base > 65534) l_base = 65534;
    l_base = (l_base * cfg.rumbleLeftScale) / 100;

    // Apply master intensity scaling percentage
    uint32_t l_scaled = (l_base * masterPct) / 100;
    uint32_t r_scaled = (r_base * masterPct) / 100;

    if (l_scaled > 65534) l_scaled = 65534;
    if (r_scaled > 65534) r_scaled = 65534;

    uint16_t left_16  = (uint16_t)l_scaled;
    uint16_t right_16 = (uint16_t)r_scaled;

    if (left_16 == s_lastRumbleLeft && right_16 == s_lastRumbleRight) {
        return;
    }
    s_lastRumbleLeft = left_16;
    s_lastRumbleRight = right_16;

    uint8_t payload[4];
    payload[0] = (uint8_t)(left_16 & 0xFF);
    payload[1] = (uint8_t)((left_16 >> 8) & 0xFF);
    payload[2] = (uint8_t)(right_16 & 0xFF);
    payload[3] = (uint8_t)((right_16 >> 8) & 0xFF);

    if (s_pRumbleChar && s_connected && s_pClient && s_pClient->isConnected()) {
        bool noResponse = s_pRumbleChar->canWriteNoResponse();
        s_pRumbleChar->writeValue(payload, sizeof(payload), !noResponse);
    }
}

void ble_gamepad_start_scan() {
    if (s_connected || s_connecting) {
        return;
    }

    NimBLEScan* pScan = NimBLEDevice::getScan();
    if (!pScan) return;

    if (pScan->isScanning()) {
        pScan->stop();
    }
    // Always clear cached scan results so previously seen MACs and heap memory
    // are never stuck or leaked over long sessions.
    pScan->clearResults();

    Serial.println("[BLE] Starting BLE Gamepad Scan (looking for Stadia / BLE gamepads)...");
    s_isScanning = true;
    s_lastScanStart = millis();
    pScan->start(0, onScanEnded, false);
}

void ble_gamepad_disconnect_and_scan() {
    s_doDisconnect = true;
}

static void bleWorkerTask(void* param) {
    (void)param;
    unsigned long lastHealthCheckMs = 0;

    for (;;) {
        unsigned long now = millis();

        if (s_doDisconnect) {
            s_doDisconnect = false;
            if (s_pClient && s_pClient->isConnected()) {
                s_pClient->disconnect();
            }
            s_connected = false;
            resetControllerState();
            vTaskDelay(pdMS_TO_TICKS(200));
            ble_gamepad_start_scan();
        }

        if (s_doConnect) {
            s_doConnect = false;
            if (!connectToTarget(s_targetAddress, s_targetName)) {
                s_lastScanStart = millis() + 1000;
            }
        }

        // Process rumble updates & motor expiry watchdog inside background worker task
        // so GATT writes never stall the 100Hz USB HID loop on Core 1.
        processPendingRumble();

        // Periodic link health check & scan refresh
        if (now - lastHealthCheckMs >= 2000) {
            lastHealthCheckMs = now;
            if (s_connected) {
                if (!s_pClient || !s_pClient->isConnected()) {
                    Serial.println("[BLE] Watchdog detected dead link, cleaning up...");
                    s_connected = false;
                    s_pRumbleChar = nullptr;
                    resetControllerState();
                    s_lastScanStart = now + 500;
                } else {
                    s_lastRssi = s_pClient->getRssi();
                }
            } else if (!s_connecting && !s_doConnect) {
                NimBLEScan* pScan = NimBLEDevice::getScan();
                if (pScan) {
                    if (!pScan->isScanning() && (long)(now - s_lastScanStart) >= 0) {
                        ble_gamepad_start_scan();
                    } else if (pScan->isScanning() && (now - s_lastScanStart >= 15000)) {
                        // Periodically flush scan cache every 15s while searching
                        pScan->clearResults();
                        s_lastScanStart = now;
                    }
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void ble_gamepad_init() {
    Serial.println("[BLE] Initializing NimBLE Bluetooth Stack...");
    NimBLEDevice::init("PS3-Bridge-Host");

    // Strong connection link power
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);

    // Standard "Just Works" pairing with bonding enabled
    NimBLEDevice::setSecurityAuth(true, false, true);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
    NimBLEDevice::setSecurityInitKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);
    NimBLEDevice::setSecurityRespKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);

    NimBLEScan* pScan = NimBLEDevice::getScan();
    // Pass wantDuplicates = true so controllers whose name arrives in Scan Response
    // or reconnect advertisements are never dropped by the controller duplicate filter.
    pScan->setAdvertisedDeviceCallbacks(&s_advCallbacks, true);
    pScan->setActiveScan(true);
    pScan->setInterval(80);
    pScan->setWindow(40);
    pScan->setMaxResults(0); // Do not store scan results in RAM vector (prevents heap leak)

    ble_gamepad_start_scan();

    // Run BLE connection & GATT write worker on Core 0 so Core 1 USB HID is never blocked
    xTaskCreatePinnedToCore(
        bleWorkerTask,
        "ble_worker",
        4096,
        nullptr,
        2,
        &s_bleWorkerTaskHandle,
        0
    );
}

void ble_gamepad_loop() {
    // Background worker task handles connection and GATT writes asynchronously.
}

bool ble_gamepad_is_connected() {
    return s_connected;
}

BleGamepadTelemetry ble_gamepad_get_telemetry() {
    BleGamepadTelemetry t;
    memset(&t, 0, sizeof(t));
    t.connected = s_connected;
    NimBLEScan* pScan = NimBLEDevice::getScan();
    t.scanning = (!s_connected && pScan && pScan->isScanning());
    t.hasRumble = (s_connected && s_pRumbleChar != nullptr);
    t.rssi = s_lastRssi;
    t.packetsReceived = s_packetCount;
    t.lastPacketAgeMs = (s_lastPacketMs > 0) ? (millis() - s_lastPacketMs) : 0;
    strncpy(t.deviceName, s_targetName.c_str(), sizeof(t.deviceName) - 1);
    std::string macStr = s_targetAddress.toString();
    strncpy(t.deviceMac, macStr.c_str(), sizeof(t.deviceMac) - 1);
    return t;
}

void ble_gamepad_print_status() {
    BleGamepadTelemetry t = ble_gamepad_get_telemetry();
    Serial.println("--- BLE Gamepad Status ---");
    Serial.printf("Connected: %s\n", t.connected ? "YES" : "NO");
    if (t.connected) {
        Serial.printf("Device:    %s (%s)\n", t.deviceName, t.deviceMac);
        Serial.printf("RSSI:      %d dBm\n", t.rssi);
        Serial.printf("Rumble:    %s\n", t.hasRumble ? "ACTIVE" : "NONE");
        Serial.printf("Packets:   %u (last %lu ms ago)\n", t.packetsReceived, t.lastPacketAgeMs);
    } else {
        Serial.printf("Scanning:  %s\n", t.scanning ? "ACTIVE" : "IDLE");
    }
    Serial.println("--------------------------");
}
