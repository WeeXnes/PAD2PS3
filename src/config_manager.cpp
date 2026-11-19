#include "config_manager.h"
#include "ble_gamepad.h"
#include <Arduino.h>
#include <Preferences.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static const uint32_t CONFIG_MAGIC   = 0x50533343; // "PS3C"
static const uint16_t CONFIG_VERSION = 2;
static const char*    NVS_NAMESPACE  = "pad2ps3";
static const char*    NVS_KEY_BLOB   = "cfg_v2";

static ControllerConfig s_activeConfig;
static portMUX_TYPE s_configMux = portMUX_INITIALIZER_UNLOCKED;

static float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static int clampi(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

void config_reset_defaults(ControllerConfig& cfg) {
    memset(&cfg, 0, sizeof(cfg));
    cfg.magic   = CONFIG_MAGIC;
    cfg.version = CONFIG_VERSION;

    // Default Left Stick: Linear, 5% inner deadzone, 2% outer saturation
    cfg.leftStick.curveType     = CURVE_LINEAR;
    cfg.leftStick.deadzoneInner = 5;
    cfg.leftStick.deadzoneOuter = 2;
    cfg.leftStick.sensitivity   = 100;
    cfg.leftStick.curveParam    = 160; // 1.60 exponent when switched to Exp/Custom
    cfg.leftStick.invertX       = false;
    cfg.leftStick.invertY       = false;

    // Default Right Stick: Linear, 5% inner deadzone, 2% outer saturation
    cfg.rightStick.curveType     = CURVE_LINEAR;
    cfg.rightStick.deadzoneInner = 5;
    cfg.rightStick.deadzoneOuter = 2;
    cfg.rightStick.sensitivity   = 100;
    cfg.rightStick.curveParam    = 160;
    cfg.rightStick.invertX       = false;
    cfg.rightStick.invertY       = false;

    cfg.swapSticks = false;

    // Default L2 & R2 Triggers
    cfg.triggerL2.curveType        = CURVE_LINEAR;
    cfg.triggerL2.deadzoneMin      = 4;
    cfg.triggerL2.deadzoneMax      = 252;
    cfg.triggerL2.digitalThreshold = 30;
    cfg.triggerL2.curveParam       = 150;

    cfg.triggerR2.curveType        = CURVE_LINEAR;
    cfg.triggerR2.deadzoneMin      = 4;
    cfg.triggerR2.deadzoneMax      = 252;
    cfg.triggerR2.digitalThreshold = 30;
    cfg.triggerR2.curveParam       = 150;

    cfg.swapTriggers = false;
    cfg.dpadMode     = DPAD_MODE_STANDARD;

    // Identity 1:1 button mapping (0..15)
    for (uint8_t i = 0; i < NUM_MAPPED_BUTTONS; i++) {
        cfg.buttonMap[i] = i;
    }

    // Rumble defaults
    cfg.rumbleIntensity  = 60;
    cfg.rumbleLeftScale  = 100;
    cfg.rumbleRightScale = 100;

    // WiFi AP defaults
    cfg.wifiEnabled = true;
    cfg.wifiChannel = 6;
    strncpy(cfg.wifiSsid, "PAD2PS3-Config", sizeof(cfg.wifiSsid) - 1);
    strncpy(cfg.wifiPassword, "ps3controller", sizeof(cfg.wifiPassword) - 1);
}

static void sanitize_config(ControllerConfig& cfg) {
    cfg.magic   = CONFIG_MAGIC;
    cfg.version = CONFIG_VERSION;

    auto sanitizeStick = [](StickCurveConfig& s) {
        if (s.curveType > CURVE_CUSTOM) s.curveType = CURVE_LINEAR;
        s.deadzoneInner = (uint8_t)clampi(s.deadzoneInner, 0, 40);
        s.deadzoneOuter = (uint8_t)clampi(s.deadzoneOuter, 0, 25);
        s.sensitivity   = (uint8_t)clampi(s.sensitivity, 50, 150);
        s.curveParam    = (uint16_t)clampi(s.curveParam, 30, 350);
    };
    sanitizeStick(cfg.leftStick);
    sanitizeStick(cfg.rightStick);

    auto sanitizeTrig = [](TriggerCurveConfig& t) {
        if (t.curveType > CURVE_CUSTOM) t.curveType = CURVE_LINEAR;
        t.deadzoneMin      = (uint8_t)clampi(t.deadzoneMin, 0, 100);
        t.deadzoneMax      = (uint8_t)clampi(t.deadzoneMax, 150, 255);
        t.digitalThreshold = (uint8_t)clampi(t.digitalThreshold, 1, 255);
        t.curveParam       = (uint16_t)clampi(t.curveParam, 30, 350);
    };
    sanitizeTrig(cfg.triggerL2);
    sanitizeTrig(cfg.triggerR2);

    if (cfg.dpadMode > DPAD_MODE_BOTH) cfg.dpadMode = DPAD_MODE_STANDARD;
    for (uint8_t i = 0; i < NUM_MAPPED_BUTTONS; i++) {
        if (cfg.buttonMap[i] >= NUM_MAPPED_BUTTONS && cfg.buttonMap[i] != BUTTON_MAP_DISABLED) {
            cfg.buttonMap[i] = i;
        }
    }

    cfg.rumbleIntensity  = (uint8_t)clampi(cfg.rumbleIntensity, 0, 100);
    cfg.rumbleLeftScale  = (uint8_t)clampi(cfg.rumbleLeftScale, 0, 100);
    cfg.rumbleRightScale = (uint8_t)clampi(cfg.rumbleRightScale, 0, 100);

    cfg.wifiChannel = (uint8_t)clampi(cfg.wifiChannel, 1, 11);
    cfg.wifiSsid[sizeof(cfg.wifiSsid) - 1]         = '\0';
    cfg.wifiPassword[sizeof(cfg.wifiPassword) - 1] = '\0';
    if (strlen(cfg.wifiSsid) == 0) {
        strncpy(cfg.wifiSsid, "PAD2PS3-Config", sizeof(cfg.wifiSsid) - 1);
    }
    // WPA2 requires >= 8 chars or empty (open network)
    size_t passLen = strlen(cfg.wifiPassword);
    if (passLen > 0 && passLen < 8) {
        strncpy(cfg.wifiPassword, "ps3controller", sizeof(cfg.wifiPassword) - 1);
    }
}

void config_init() {
    ControllerConfig defaults;
    config_reset_defaults(defaults);
    portENTER_CRITICAL(&s_configMux);
    s_activeConfig = defaults;
    portEXIT_CRITICAL(&s_configMux);

    if (config_load()) {
        Serial.println("[CFG] Loaded controller configuration from NVS flash.");
    } else {
        Serial.println("[CFG] Using default controller configuration.");
    }
}

bool config_load() {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, true)) {
        return false;
    }

    ControllerConfig loaded;
    size_t readLen = prefs.getBytes(NVS_KEY_BLOB, &loaded, sizeof(ControllerConfig));
    prefs.end();

    if (readLen != sizeof(ControllerConfig) ||
        loaded.magic != CONFIG_MAGIC ||
        loaded.version != CONFIG_VERSION) {
        return false;
    }

    sanitize_config(loaded);

    portENTER_CRITICAL(&s_configMux);
    s_activeConfig = loaded;
    portEXIT_CRITICAL(&s_configMux);

    ble_gamepad_set_rumble_intensity(loaded.rumbleIntensity);
    return true;
}

bool config_save() {
    ControllerConfig copy = get_config_copy();

    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) {
        Serial.println("[CFG] ERROR: Failed to open NVS for writing!");
        return false;
    }

    size_t written = prefs.putBytes(NVS_KEY_BLOB, &copy, sizeof(ControllerConfig));
    prefs.end();

    bool ok = (written == sizeof(ControllerConfig));
    Serial.printf("[CFG] Saved configuration to NVS (%s)\n", ok ? "OK" : "FAILED");
    return ok;
}

ControllerConfig get_config_copy() {
    ControllerConfig copy;
    portENTER_CRITICAL(&s_configMux);
    copy = s_activeConfig;
    portEXIT_CRITICAL(&s_configMux);
    return copy;
}

void set_config(const ControllerConfig& cfg, bool saveToNvs) {
    ControllerConfig clean = cfg;
    sanitize_config(clean);

    portENTER_CRITICAL(&s_configMux);
    s_activeConfig = clean;
    portEXIT_CRITICAL(&s_configMux);

    ble_gamepad_set_rumble_intensity(clean.rumbleIntensity);

    if (saveToNvs) {
        config_save();
    }
}

// ---------------------------------------------------------------------------
// Response Curve Evaluation Math
// ---------------------------------------------------------------------------
float evaluate_normalized_curve(float x, uint8_t curveType, uint16_t curveParam) {
    x = clampf(x, 0.0f, 1.0f);
    if (x <= 0.0f) return 0.0f;
    if (x >= 1.0f) return 1.0f;

    float param = clampf((float)curveParam / 100.0f, 0.30f, 3.50f);
    float y = x;

    switch (curveType) {
        case CURVE_LINEAR:
            y = x;
            break;

        case CURVE_EXPONENTIAL: {
            // Precision curve: gentler slope near center, steeper near edge
            // Uses curveParam (default 1.60, or >= 1.1)
            float expVal = (param < 1.05f) ? 1.65f : param;
            y = powf(x, expVal);
            break;
        }

        case CURVE_S_CURVE: {
            // Smooth S-curve (blends smoothstep with adjustable steepness)
            // Gives fine micro-adjustments around center AND smooth approach at outer edge
            float smooth = x * x * (3.0f - 2.0f * x);
            float blend = clampf((param - 0.5f) / 2.0f, 0.2f, 1.0f);
            y = x * (1.0f - blend) + smooth * blend;
            break;
        }

        case CURVE_AGGRESSIVE: {
            // Fast / hair-trigger response: steeper initial slope (exponent < 1.0)
            float fastExp = (param > 0.95f) ? (1.0f / param) : param;
            fastExp = clampf(fastExp, 0.35f, 0.85f);
            y = powf(x, fastExp);
            break;
        }

        case CURVE_CUSTOM: {
            // Direct power curve using exact user exponent (0.30 .. 3.50)
            y = powf(x, param);
            break;
        }

        default:
            y = x;
            break;
    }

    return clampf(y, 0.0f, 1.0f);
}

void apply_stick_curve_2d(uint8_t rawX, uint8_t rawY, const StickCurveConfig& cfg, uint8_t& outX, uint8_t& outY) {
    // Normalize 0..255 (center 128) into -1.0 .. +1.0
    float dx = ((int)rawX - 128) / 127.0f;
    float dy = ((int)rawY - 128) / 127.0f;
    dx = clampf(dx, -1.0f, 1.0f);
    dy = clampf(dy, -1.0f, 1.0f);

    if (cfg.invertX) dx = -dx;
    if (cfg.invertY) dy = -dy;

    float mag = sqrtf(dx * dx + dy * dy);
    float dzIn  = (float)cfg.deadzoneInner / 100.0f;
    float dzOut = 1.0f - ((float)cfg.deadzoneOuter / 100.0f);
    if (dzOut <= dzIn + 0.05f) {
        dzOut = dzIn + 0.05f;
    }

    if (mag <= dzIn || mag < 0.0001f) {
        outX = 128;
        outY = 128;
        return;
    }

    // Unit direction vector preserving exact 360-degree stick angle
    float ux = dx / mag;
    float uy = dy / mag;

    // Rescale magnitude between inner deadzone and outer saturation ring
    float normMag = (mag - dzIn) / (dzOut - dzIn);
    normMag = clampf(normMag, 0.0f, 1.0f);

    // Apply activation curve
    float curvedMag = evaluate_normalized_curve(normMag, cfg.curveType, cfg.curveParam);

    // Apply sensitivity scaling
    curvedMag *= ((float)cfg.sensitivity / 100.0f);
    curvedMag = clampf(curvedMag, 0.0f, 1.0f);

    int finalX = (int)roundf(128.0f + ux * curvedMag * 127.0f);
    int finalY = (int)roundf(128.0f + uy * curvedMag * 127.0f);

    outX = (uint8_t)clampi(finalX, 0, 255);
    outY = (uint8_t)clampi(finalY, 0, 255);
}

uint8_t apply_trigger_curve(uint8_t rawVal, const TriggerCurveConfig& cfg) {
    if (rawVal <= cfg.deadzoneMin) {
        return 0;
    }
    uint8_t maxVal = (cfg.deadzoneMax > cfg.deadzoneMin + 5) ? cfg.deadzoneMax : (cfg.deadzoneMin + 5);
    if (rawVal >= maxVal) {
        return 255;
    }

    float norm = (float)(rawVal - cfg.deadzoneMin) / (float)(maxVal - cfg.deadzoneMin);
    float curved = evaluate_normalized_curve(norm, cfg.curveType, cfg.curveParam);
    int out = (int)roundf(curved * 255.0f);
    return (uint8_t)clampi(out, 0, 255);
}

// ---------------------------------------------------------------------------
// Minimal Zero-Dependency JSON Serializer & Parser for Web UI
// ---------------------------------------------------------------------------
size_t config_to_json(char* buf, size_t maxLen) {
    ControllerConfig c = get_config_copy();

    int n = snprintf(
        buf, maxLen,
        "{"
        "\"leftStick\":{\"curveType\":%u,\"deadzoneInner\":%u,\"deadzoneOuter\":%u,\"sensitivity\":%u,\"curveParam\":%u,\"invertX\":%s,\"invertY\":%s},"
        "\"rightStick\":{\"curveType\":%u,\"deadzoneInner\":%u,\"deadzoneOuter\":%u,\"sensitivity\":%u,\"curveParam\":%u,\"invertX\":%s,\"invertY\":%s},"
        "\"swapSticks\":%s,"
        "\"triggerL2\":{\"curveType\":%u,\"deadzoneMin\":%u,\"deadzoneMax\":%u,\"digitalThreshold\":%u,\"curveParam\":%u},"
        "\"triggerR2\":{\"curveType\":%u,\"deadzoneMin\":%u,\"deadzoneMax\":%u,\"digitalThreshold\":%u,\"curveParam\":%u},"
        "\"swapTriggers\":%s,"
        "\"dpadMode\":%u,"
        "\"buttonMap\":[%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u],"
        "\"rumbleIntensity\":%u,"
        "\"rumbleLeftScale\":%u,"
        "\"rumbleRightScale\":%u,"
        "\"wifiEnabled\":%s,"
        "\"wifiChannel\":%u,"
        "\"wifiSsid\":\"%s\""
        "}",
        c.leftStick.curveType, c.leftStick.deadzoneInner, c.leftStick.deadzoneOuter,
        c.leftStick.sensitivity, c.leftStick.curveParam,
        c.leftStick.invertX ? "true" : "false", c.leftStick.invertY ? "true" : "false",

        c.rightStick.curveType, c.rightStick.deadzoneInner, c.rightStick.deadzoneOuter,
        c.rightStick.sensitivity, c.rightStick.curveParam,
        c.rightStick.invertX ? "true" : "false", c.rightStick.invertY ? "true" : "false",

        c.swapSticks ? "true" : "false",

        c.triggerL2.curveType, c.triggerL2.deadzoneMin, c.triggerL2.deadzoneMax,
        c.triggerL2.digitalThreshold, c.triggerL2.curveParam,

        c.triggerR2.curveType, c.triggerR2.deadzoneMin, c.triggerR2.deadzoneMax,
        c.triggerR2.digitalThreshold, c.triggerR2.curveParam,

        c.swapTriggers ? "true" : "false",
        c.dpadMode,
        c.buttonMap[0], c.buttonMap[1], c.buttonMap[2], c.buttonMap[3],
        c.buttonMap[4], c.buttonMap[5], c.buttonMap[6], c.buttonMap[7],
        c.buttonMap[8], c.buttonMap[9], c.buttonMap[10], c.buttonMap[11],
        c.buttonMap[12], c.buttonMap[13], c.buttonMap[14], c.buttonMap[15],
        c.rumbleIntensity,
        c.rumbleLeftScale,
        c.rumbleRightScale,
        c.wifiEnabled ? "true" : "false",
        c.wifiChannel,
        c.wifiSsid
    );

    return (n > 0 && (size_t)n < maxLen) ? (size_t)n : 0;
}

static const char* find_key_in_range(const char* start, const char* end, const char* key) {
    char pattern[48];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    size_t patLen = strlen(pattern);

    const char* p = start;
    while (p && p + patLen < end) {
        const char* match = strstr(p, pattern);
        if (!match || match >= end) return nullptr;
        const char* colon = match + patLen;
        while (colon < end && (*colon == ' ' || *colon == '\t' || *colon == '\r' || *colon == '\n')) colon++;
        if (colon < end && *colon == ':') {
            colon++;
            while (colon < end && (*colon == ' ' || *colon == '\t' || *colon == '\r' || *colon == '\n')) colon++;
            return colon;
        }
        p = match + patLen;
    }
    return nullptr;
}

static bool parse_int_in_range(const char* start, const char* end, const char* key, int& outVal) {
    const char* valPtr = find_key_in_range(start, end, key);
    if (!valPtr) return false;
    outVal = atoi(valPtr);
    return true;
}

static bool parse_bool_in_range(const char* start, const char* end, const char* key, bool& outVal) {
    const char* valPtr = find_key_in_range(start, end, key);
    if (!valPtr) return false;
    if (strncmp(valPtr, "true", 4) == 0 || *valPtr == '1') {
        outVal = true;
        return true;
    }
    if (strncmp(valPtr, "false", 5) == 0 || *valPtr == '0') {
        outVal = false;
        return true;
    }
    return false;
}

static bool parse_str_in_range(const char* start, const char* end, const char* key, char* outBuf, size_t maxBuf) {
    const char* valPtr = find_key_in_range(start, end, key);
    if (!valPtr || *valPtr != '"') return false;
    valPtr++;
    size_t idx = 0;
    while (valPtr < end && *valPtr && *valPtr != '"' && idx < maxBuf - 1) {
        if (*valPtr == '\\' && (valPtr + 1) < end) {
            valPtr++;
        }
        outBuf[idx++] = *valPtr++;
    }
    outBuf[idx] = '\0';
    return true;
}

static void parse_stick_obj(const char* json, const char* end, const char* objKey, StickCurveConfig& stick) {
    const char* objStart = find_key_in_range(json, end, objKey);
    if (!objStart || *objStart != '{') return;
    const char* objEnd = strchr(objStart, '}');
    if (!objEnd || objEnd > end) return;

    int v = 0;
    if (parse_int_in_range(objStart, objEnd, "curveType", v))     stick.curveType     = (uint8_t)v;
    if (parse_int_in_range(objStart, objEnd, "deadzoneInner", v)) stick.deadzoneInner = (uint8_t)v;
    if (parse_int_in_range(objStart, objEnd, "deadzoneOuter", v)) stick.deadzoneOuter = (uint8_t)v;
    if (parse_int_in_range(objStart, objEnd, "sensitivity", v))   stick.sensitivity   = (uint8_t)v;
    if (parse_int_in_range(objStart, objEnd, "curveParam", v))    stick.curveParam    = (uint16_t)v;
    parse_bool_in_range(objStart, objEnd, "invertX", stick.invertX);
    parse_bool_in_range(objStart, objEnd, "invertY", stick.invertY);
}

static void parse_trigger_obj(const char* json, const char* end, const char* objKey, TriggerCurveConfig& trig) {
    const char* objStart = find_key_in_range(json, end, objKey);
    if (!objStart || *objStart != '{') return;
    const char* objEnd = strchr(objStart, '}');
    if (!objEnd || objEnd > end) return;

    int v = 0;
    if (parse_int_in_range(objStart, objEnd, "curveType", v))        trig.curveType        = (uint8_t)v;
    if (parse_int_in_range(objStart, objEnd, "deadzoneMin", v))      trig.deadzoneMin      = (uint8_t)v;
    if (parse_int_in_range(objStart, objEnd, "deadzoneMax", v))      trig.deadzoneMax      = (uint8_t)v;
    if (parse_int_in_range(objStart, objEnd, "digitalThreshold", v)) trig.digitalThreshold = (uint8_t)v;
    if (parse_int_in_range(objStart, objEnd, "curveParam", v))       trig.curveParam       = (uint16_t)v;
}

bool config_update_from_json(const char* jsonStr, bool saveToNvs) {
    if (!jsonStr) return false;
    size_t len = strlen(jsonStr);
    if (len < 2) return false;
    const char* end = jsonStr + len;

    ControllerConfig cfg = get_config_copy();

    parse_stick_obj(jsonStr, end, "leftStick", cfg.leftStick);
    parse_stick_obj(jsonStr, end, "rightStick", cfg.rightStick);
    parse_trigger_obj(jsonStr, end, "triggerL2", cfg.triggerL2);
    parse_trigger_obj(jsonStr, end, "triggerR2", cfg.triggerR2);

    parse_bool_in_range(jsonStr, end, "swapSticks", cfg.swapSticks);
    parse_bool_in_range(jsonStr, end, "swapTriggers", cfg.swapTriggers);

    int v = 0;
    if (parse_int_in_range(jsonStr, end, "dpadMode", v))         cfg.dpadMode         = (uint8_t)v;
    if (parse_int_in_range(jsonStr, end, "rumbleIntensity", v))  cfg.rumbleIntensity  = (uint8_t)v;
    if (parse_int_in_range(jsonStr, end, "rumbleLeftScale", v))  cfg.rumbleLeftScale  = (uint8_t)v;
    if (parse_int_in_range(jsonStr, end, "rumbleRightScale", v)) cfg.rumbleRightScale = (uint8_t)v;
    if (parse_int_in_range(jsonStr, end, "wifiChannel", v))      cfg.wifiChannel      = (uint8_t)v;
    parse_bool_in_range(jsonStr, end, "wifiEnabled", cfg.wifiEnabled);

    char ssidBuf[32];
    if (parse_str_in_range(jsonStr, end, "wifiSsid", ssidBuf, sizeof(ssidBuf)) && strlen(ssidBuf) > 0) {
        strncpy(cfg.wifiSsid, ssidBuf, sizeof(cfg.wifiSsid) - 1);
    }

    char passBuf[32];
    if (parse_str_in_range(jsonStr, end, "wifiPassword", passBuf, sizeof(passBuf))) {
        if (strlen(passBuf) >= 8 || strlen(passBuf) == 0) {
            strncpy(cfg.wifiPassword, passBuf, sizeof(cfg.wifiPassword) - 1);
        }
    }

    // Parse buttonMap array: "buttonMap":[0,1,2,...]
    const char* mapPtr = find_key_in_range(jsonStr, end, "buttonMap");
    if (mapPtr && *mapPtr == '[') {
        mapPtr++;
        for (size_t i = 0; i < NUM_MAPPED_BUTTONS && mapPtr < end; i++) {
            while (mapPtr < end && (*mapPtr == ' ' || *mapPtr == '\t' || *mapPtr == '\r' || *mapPtr == '\n')) mapPtr++;
            if (mapPtr >= end || *mapPtr == ']') break;
            int btnTarget = atoi(mapPtr);
            if (btnTarget < 0 || btnTarget >= 255) {
                cfg.buttonMap[i] = BUTTON_MAP_DISABLED;
            } else {
                cfg.buttonMap[i] = (uint8_t)btnTarget;
            }
            const char* comma = strchr(mapPtr, ',');
            const char* closeBracket = strchr(mapPtr, ']');
            if (!comma || (closeBracket && comma > closeBracket)) break;
            mapPtr = comma + 1;
        }
    }

    set_config(cfg, saveToNvs);
    return true;
}
