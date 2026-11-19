#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "controller_state.h"

/**
 * Analog Response Curve Types
 */
enum ResponseCurveType : uint8_t {
    CURVE_LINEAR      = 0, // 1:1 linear response
    CURVE_EXPONENTIAL = 1, // Fine precision near center, power ramp toward edge
    CURVE_S_CURVE     = 2, // Smooth center & edge with responsive mid-range
    CURVE_AGGRESSIVE  = 3, // Fast initial ramp / instant response
    CURVE_CUSTOM      = 4  // Custom exponent controlled by curveParam
};

/**
 * D-Pad Mapping Modes
 */
enum DpadMapMode : uint8_t {
    DPAD_MODE_STANDARD   = 0, // D-Pad outputs to PS3 D-Pad
    DPAD_MODE_LEFT_STICK = 1, // D-Pad drives Left Analog Stick
    DPAD_MODE_BOTH       = 2  // D-Pad drives both PS3 D-Pad and Left Stick
};

/**
 * Per-Stick Configuration (Left or Right Analog Stick)
 */
struct StickCurveConfig {
    uint8_t  curveType;      // ResponseCurveType (0..4)
    uint8_t  deadzoneInner;  // Inner radial deadzone percentage (0..40, default 5)
    uint8_t  deadzoneOuter;  // Outer saturation percentage (0..25, default 2)
    uint8_t  sensitivity;    // Output scaling percentage (50..150, default 100)
    uint16_t curveParam;     // Curve exponent/intensity x100 (e.g. 160 = 1.60, range 30..350)
    bool     invertX;        // Invert horizontal axis
    bool     invertY;        // Invert vertical axis
};

/**
 * Per-Trigger Configuration (L2 or R2 Analog Trigger)
 */
struct TriggerCurveConfig {
    uint8_t  curveType;        // ResponseCurveType (0..4)
    uint8_t  deadzoneMin;      // Raw minimum threshold (0..100, default 5)
    uint8_t  deadzoneMax;      // Raw maximum saturation (150..255, default 250)
    uint8_t  digitalThreshold; // Digital L2/R2 button actuation point (1..255, default 30)
    uint16_t curveParam;       // Curve exponent/intensity x100 (e.g. 150 = 1.50, range 30..350)
};

#define BUTTON_MAP_DISABLED 0xFF
#define NUM_MAPPED_BUTTONS  16

/**
 * Complete User Configuration Structure (persisted in ESP32 NVS Flash)
 */
struct ControllerConfig {
    uint32_t magic;
    uint16_t version;

    // Analog Sticks
    StickCurveConfig leftStick;
    StickCurveConfig rightStick;
    bool swapSticks;

    // Analog Triggers
    TriggerCurveConfig triggerL2;
    TriggerCurveConfig triggerR2;
    bool swapTriggers;

    // D-Pad Mode & Button Remapping (physical index 0..15 -> target ControllerButton 0..15 or 0xFF)
    uint8_t dpadMode;
    uint8_t buttonMap[NUM_MAPPED_BUTTONS];

    // Rumble / Haptics
    uint8_t rumbleIntensity;   // Master intensity 0..100% (default 60)
    uint8_t rumbleLeftScale;   // Big/Heavy motor scale 0..100% (default 100)
    uint8_t rumbleRightScale;  // Small/Light motor scale 0..100% (default 100)

    // WiFi AP Settings
    bool    wifiEnabled;
    uint8_t wifiChannel;
    char    wifiSsid[32];
    char    wifiPassword[32];
};

// Lifecycle & Persistence API
void config_init();
void config_reset_defaults(ControllerConfig& cfg);
bool config_load();
bool config_save();

// Thread-safe accessors
ControllerConfig get_config_copy();
void set_config(const ControllerConfig& cfg, bool saveToNvs = false);

// Curve evaluation & transformation helpers
float evaluate_normalized_curve(float x, uint8_t curveType, uint16_t curveParam);
void apply_stick_curve_2d(uint8_t rawX, uint8_t rawY, const StickCurveConfig& cfg, uint8_t& outX, uint8_t& outY);
uint8_t apply_trigger_curve(uint8_t rawVal, const TriggerCurveConfig& cfg);

// JSON Serialization & Deserialization for Web UI API
size_t config_to_json(char* buf, size_t maxLen);
bool config_update_from_json(const char* jsonStr, bool saveToNvs = false);

#endif // CONFIG_MANAGER_H
