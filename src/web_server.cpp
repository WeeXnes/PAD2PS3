#include "web_server.h"
#include "web_ui.h"
#include "config_manager.h"
#include "controller_state.h"
#include "ble_gamepad.h"
#include "usb_hid.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <esp_wifi.h>

#if __has_include(<esp_coexist.h>)
#include <esp_coexist.h>
#define HAS_ESP_COEXIST 1
#else
#define HAS_ESP_COEXIST 0
#endif

static WebServer s_server(80);
static DNSServer s_dnsServer;
static bool s_running = false;
static TaskHandle_t s_webTaskHandle = nullptr;
static volatile unsigned long s_pendingPsPulseMs = 0;

static const IPAddress AP_LOCAL_IP(192, 168, 4, 1);
static const IPAddress AP_GATEWAY(192, 168, 4, 1);
static const IPAddress AP_SUBNET(255, 255, 255, 0);

bool web_server_consume_ps_pulse(unsigned long& outDurationMs) {
    unsigned long dur = s_pendingPsPulseMs;
    if (dur > 0) {
        s_pendingPsPulseMs = 0;
        outDurationMs = dur;
        return true;
    }
    return false;
}

static void handleRoot() {
    s_server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    s_server.send_P(200, "text/html", WEB_UI_HTML);
}

static void handleGetStatus() {
    BleGamepadTelemetry ble = ble_gamepad_get_telemetry();
    ControllerState raw = getRawControllerStateCopy();
    ControllerState out = getControllerStateCopy();

    // Escape device name safely
    char safeName[48];
    size_t j = 0;
    for (size_t i = 0; ble.deviceName[i] && j < sizeof(safeName) - 2; i++) {
        char c = ble.deviceName[i];
        if (c == '"' || c == '\\') safeName[j++] = '\\';
        if ((unsigned char)c >= 32 && (unsigned char)c <= 126) {
            safeName[j++] = c;
        }
    }
    safeName[j] = '\0';
    if (j == 0) strncpy(safeName, "BLE Gamepad", sizeof(safeName) - 1);

    char json[512];
    snprintf(
        json, sizeof(json),
        "{"
        "\"uptime\":%lu,"
        "\"ble\":{\"connected\":%s,\"scanning\":%s,\"rumble\":%s,\"rssi\":%d,\"packets\":%u,\"name\":\"%s\",\"mac\":\"%s\"},"
        "\"usb\":{\"mounted\":%s,\"paired\":%s,\"player\":%d,\"reports\":%u},"
        "\"raw\":{\"lx\":%u,\"ly\":%u,\"rx\":%u,\"ry\":%u,\"l2\":%u,\"r2\":%u,\"b1\":%u,\"b2\":%u,\"dpad\":%u},"
        "\"out\":{\"lx\":%u,\"ly\":%u,\"rx\":%u,\"ry\":%u,\"l2\":%u,\"r2\":%u,\"b1\":%u,\"b2\":%u,\"dpad\":%u}"
        "}",
        millis() / 1000UL,
        ble.connected ? "true" : "false",
        ble.scanning ? "true" : "false",
        ble.hasRumble ? "true" : "false",
        ble.rssi,
        ble.packetsReceived,
        safeName,
        ble.deviceMac,
        usb_hid_is_mounted() ? "true" : "false",
        usb_hid_is_paired() ? "true" : "false",
        usb_hid_get_assigned_player(),
        usb_hid_get_reports_sent(),
        raw.leftX, raw.leftY, raw.rightX, raw.rightY, raw.L2, raw.R2, raw.buttons1, raw.buttons2, raw.dpad,
        out.leftX, out.leftY, out.rightX, out.rightY, out.L2, out.R2, out.buttons1, out.buttons2, out.dpad
    );

    s_server.sendHeader("Access-Control-Allow-Origin", "*");
    s_server.send(200, "application/json", json);
}

static void handleGetConfig() {
    char json[1024];
    config_to_json(json, sizeof(json));
    s_server.sendHeader("Access-Control-Allow-Origin", "*");
    s_server.send(200, "application/json", json);
}

static void handlePostConfig() {
    if (!s_server.hasArg("plain")) {
        s_server.send(400, "application/json", "{\"ok\":false,\"err\":\"Empty body\"}");
        return;
    }

    bool saveToFlash = (s_server.hasArg("save") && s_server.arg("save") == "1");
    String body = s_server.arg("plain");
    bool ok = config_update_from_json(body.c_str(), saveToFlash);
    if (ok) {
        reapplyActiveConfigToState();
    }

    char json[1024];
    config_to_json(json, sizeof(json));
    s_server.sendHeader("Access-Control-Allow-Origin", "*");
    s_server.send(ok ? 200 : 400, "application/json", json);
}

static void handlePostReset() {
    ControllerConfig def;
    config_reset_defaults(def);
    set_config(def, true);
    reapplyActiveConfigToState();

    char json[1024];
    config_to_json(json, sizeof(json));
    s_server.sendHeader("Access-Control-Allow-Origin", "*");
    s_server.send(200, "application/json", json);
}

static void handlePostAction() {
    String cmd = s_server.arg("cmd");
    if (cmd == "scan") {
        ble_gamepad_disconnect_and_scan();
    } else if (cmd == "rumble_test") {
        ble_gamepad_set_rumble(1, 220, 450, 450);
    } else if (cmd == "ps_pulse") {
        s_pendingPsPulseMs = 220;
    } else if (cmd == "ps_hold") {
        s_pendingPsPulseMs = 1500;
    }
    s_server.sendHeader("Access-Control-Allow-Origin", "*");
    s_server.send(200, "application/json", "{\"ok\":true}");
}

static void webServerTask(void* param) {
    (void)param;
    for (;;) {
        if (s_running) {
            s_dnsServer.processNextRequest();
            s_server.handleClient();
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void web_server_init() {
    ControllerConfig cfg = get_config_copy();
    if (!cfg.wifiEnabled) {
        Serial.println("[WIFI] Hotspot disabled in configuration.");
        return;
    }

    Serial.println("[WIFI] Starting SoftAP Hotspot & Web UI Server...");

    WiFi.persistent(false);
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(AP_LOCAL_IP, AP_GATEWAY, AP_SUBNET);

    // Coexistence & low-latency optimizations for simultaneous BLE + WiFi AP on ESP32-S3:
    // 1. Modem power save is required by ESP-IDF coexistence manager when sharing radio with BLE
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    // 2. Use 20MHz channel bandwidth to minimize RF air time and spectral overlap with BLE
    esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT20);

#if HAS_ESP_COEXIST
    // 3. Give Bluetooth hardware RF priority over WiFi so gamepad polling has zero jitter
    esp_coex_preference_set(ESP_COEX_PREFER_BT);
#endif

    const char* pass = (strlen(cfg.wifiPassword) >= 8) ? cfg.wifiPassword : nullptr;
    bool apStarted = WiFi.softAP(cfg.wifiSsid, pass, cfg.wifiChannel, 0, 4);
    // Moderate TX power (15dBm) prevents local LNA saturation on BLE receiver while giving strong room coverage
    WiFi.setTxPower(WIFI_POWER_15dBm);

    if (!apStarted) {
        Serial.println("[WIFI] ERROR: Failed to start SoftAP!");
        return;
    }

    // Start Captive Portal DNS server (redirects domains to 192.168.4.1)
    s_dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    s_dnsServer.start(53, "*", AP_LOCAL_IP);

    // HTTP Routes
    s_server.on("/", HTTP_GET, handleRoot);
    s_server.on("/index.html", HTTP_GET, handleRoot);
    s_server.on("/api/status", HTTP_GET, handleGetStatus);
    s_server.on("/api/config", HTTP_GET, handleGetConfig);
    s_server.on("/api/config", HTTP_POST, handlePostConfig);
    s_server.on("/api/reset", HTTP_POST, handlePostReset);
    s_server.on("/api/action", HTTP_POST, handlePostAction);

    // Captive portal probe routes (Android / iOS / Windows)
    s_server.on("/generate_204", HTTP_GET, handleRoot);
    s_server.on("/hotspot-detect.html", HTTP_GET, handleRoot);
    s_server.on("/ncsi.txt", HTTP_GET, handleRoot);
    s_server.onNotFound(handleRoot);

    s_server.begin();
    s_running = true;

    // Pin Web & DNS server loop to Core 0 at low priority (1) so Core 1 100Hz USB HID is untouched
    xTaskCreatePinnedToCore(
        webServerTask,
        "web_ui_task",
        6144,
        nullptr,
        1,
        &s_webTaskHandle,
        0
    );

    web_server_print_status();
}

bool web_server_is_running() {
    return s_running;
}

void web_server_print_status() {
    ControllerConfig cfg = get_config_copy();
    Serial.println("=================== WIFI WEB UI ===================");
    if (s_running) {
        Serial.printf("  Status:   ACTIVE (SoftAP + Captive Portal)\n");
        Serial.printf("  SSID:     %s\n", cfg.wifiSsid);
        Serial.printf("  Password: %s\n", strlen(cfg.wifiPassword) >= 8 ? cfg.wifiPassword : "(Open)");
        Serial.printf("  Channel:  %u\n", cfg.wifiChannel);
        Serial.printf("  Web URL:  http://%s/\n", WiFi.softAPIP().toString().c_str());
        Serial.printf("  Clients:  %d connected\n", WiFi.softAPgetStationNum());
    } else {
        Serial.println("  Status:   DISABLED");
    }
    Serial.println("===================================================");
}
