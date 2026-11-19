# PAD2PS3

ESP32-S3 firmware that emulates a Sony PlayStation 3 DualShock 3 over USB while bridging wireless BLE gamepads, such as the Google Stadia controller.

[![Platform](https://img.shields.io/badge/platform-ESP32--S3-000000?style=flat-square&labelColor=1f1f1f)](#hardware-setup)
[![Stack](https://img.shields.io/badge/stack-PlatformIO%20%C2%B7%20C%2B%2B-000000?style=flat-square&labelColor=1f1f1f)](#build-and-flash)

PAD2PS3 v2.0 (ESP32-S3 DualShock 3 Emulator and Wireless BLE Controller Bridge) is a high-performance, low-latency
firmware for the **ESP32-S3** microcontroller. It emulates an authentic **Sony PlayStation 3 DualShock 3** controller
over native USB OTG while connecting wirelessly to Bluetooth Low Energy gamepads. It is tailored for the
**Google Stadia Controller** (Bluetooth mode) and works with other BLE HID gamepads.

It turns your Google Stadia controller (or other BLE gamepads) into a native, fully functional PS3 controller with
customizable **2D radial analog stick curves**, **trigger activation curves**, **full button remapping**,
a **WiFi hotspot with an interactive Web UI**, **NVS flash persistence**, an interactive Serial CLI, and
**bidirectional rumble/haptic feedback**.

## Features

- **Authentic Sony DualShock 3 USB emulation:**
  - Emulates an authentic Sony DualShock 3 (`054C:0268`) on the ESP32-S3 native USB OTG peripheral (GPIO 19 `D-` / GPIO 20 `D+`).
  - Implements the genuine PS3 console handshake: responds to proprietary Feature Reports (`0x01`, `0xF2`, `0xF5`, `0xEF`, `0xF8`, `0xF7`, and `0xF4` streaming activation).
  - Automatically pulses the PS button when the handshake completes (and across PS3 game launches/mode transitions) to assign the controller player number (`Player 1..4` LED detection).
  - Streams standard 48-byte native DualShock 3 reports at **100Hz (10ms)** directly via TinyUSB hardware endpoints without blocking semaphore deadlocks.
- **WiFi hotspot and live web configuration UI:**
  - Hosts a built-in WiFi SoftAP (`PAD2PS3-Config`, default password `ps3controller`, IP `http://192.168.4.1`) with captive portal support.
  - Runs asynchronously on **Core 0** with hardware Bluetooth RF coexistence priority (`ESP_COEX_PREFER_BT`), so controller polling on **Core 1** keeps zero jitter.
  - **Real-time telemetry dashboard:**
    - Live 2D radar visualizers showing both **Raw Input** and **Curved PS3 Output** stick positions.
    - Live L2/R2 trigger progress bars with digital actuation indicators and real-time button press highlights.
  - **2D radial analog stick activation curves** (independent left and right stick tuning):
    - Presets: **Linear**, **Exponential (Fine Aim Precision)**, **S-Curve (Smooth Center & Edge)**, **Aggressive (Quick Ramp)**, and **Custom Power Exponent**.
    - Configurable Inner Deadzone (`0–40%`), Outer Saturation (`0–25%`), Sensitivity (`50–150%`), X/Y Axis Inversion, and Left/Right Stick Swap.
  - **Trigger activation curves (L2 and R2):**
    - Presets: **Linear**, **Soft / Racing**, **S-Curve**, **Hair Trigger / Shooter**, and **Custom**.
    - Adjustable Min Deadzone, Max Saturation, Digital Click Threshold (`1–255`), and L2/R2 Swap.
  - **Full button remapping and D-pad modes:**
    - Remap any physical button (including the Stadia **Assistant** and **Capture** buttons) to any PS3 button, or apply one-click presets (**Default**, **Nintendo Swap**, **PS3 Tactical Shooter L1↔L2 / R1↔R2**).
    - Configurable D-pad mode: **Standard D-Pad**, **D-Pad Emulates Left Analog Stick**, or **Both**.
  - **Persistent NVS storage and JSON profiles:**
    - Save configurations permanently to ESP32 NVS flash, export/import `.json` profiles, or restore factory defaults with one click.
- **Wireless BLE gamepad host (Google Stadia and BLE controllers):**
  - Scans, connects, and bonds automatically to BLE HID gamepads using NimBLE.
  - A dedicated background FreeRTOS worker task on Core 0 handles the BLE connection, fast reconnection, and GATT writes without ever blocking USB HID report streaming.
- **Bidirectional rumble / haptic feedback:**
  - Intercepts PS3 host Output Report `0x01` motor commands and durations in real time, with an automatic motor expiration watchdog.
  - Independent scaling for Master Intensity (`0–100%`), Left Heavy Motor (`0–100%`), and Right Light Motor (`0–100%`).

## Requirements

### Hardware setup

```text
 +-----------------------------------+
 |         ESP32-S3 DevKitC-1        |
 |                                   |
 |  [UART Type-C]     [USB Type-C]   |
 +-------+-------------------+-------+
         |                   |
         |                   +-----> Connect directly to PS3 Console USB port
         |                           (Uses native USB OTG: GPIO 19 D- / GPIO 20 D+)
         |
         +-------------------------> Connect to PC for Serial CLI & Flashing
                                     (Uses onboard USB-to-UART bridge)
```

> [!IMPORTANT]
> The ESP32-S3 DevKitC-1 has **two Type-C USB ports**:
> 1. **`USB` / `OTG` port:** connects directly to the **PlayStation 3** console (or PC).
> 2. **`UART` port:** connects to your computer for PlatformIO flashing and the interactive Serial CLI monitor.

## Install

### Build and flash

Connect your PC to the ESP32-S3 **`UART`** port:

```bash
# Build the project
pio run

# Flash to the ESP32-S3
pio run -t upload

# Open the serial monitor
pio device monitor
```

### Connect to the Web UI (WiFi hotspot)

1. On your phone or PC, connect to the WiFi network:
   - **SSID:** `PAD2PS3-Config`
   - **Password:** `ps3controller`
2. Open your browser to **`http://192.168.4.1`** (or use the automatic captive portal popup).
3. Customize analog stick curves, trigger curves, deadzones, button mappings and rumble strength in real time, then click **Save to Flash (NVS)**.

### Pair your wireless BLE controller

1. Put your Google Stadia controller into pairing mode by holding the **Stadia button** + **`Y`** for 3 seconds until the LED flashes **orange**.
2. The ESP32-S3 detects, pairs, bonds and subscribes to input reports automatically.

### Connect to the PS3 console

1. Connect the ESP32-S3 **`USB` (OTG)** port to the PS3 console.
2. The ESP32-S3 performs the authentic Sony handshake, pulses the PS button, and assigns `Player 1..4`.

## Usage

### Serial CLI reference

Send commands over the hardware UART serial monitor (`115200` baud, newline terminated):

| Command | Description |
| :-- | :-- |
| `help` / `?` | Print the list of all available commands |
| `status` | Print the current controller input state (buttons, axes, d-pad) |
| `wifi` | Print the WiFi hotspot SSID, IP URL, and connected client count |
| `config` | Print the active JSON configuration |
| `resetcfg` | Restore factory default settings and save to NVS |
| `log` / `debug` | Toggle live real-time input logging on/off |
| `intensity <0-100>` | Set the master rumble intensity percentage and save to NVS |
| `rumble [0-255]` | Trigger a 500ms test rumble pulse on the connected gamepad |
| `ble` | Print the current Bluetooth / BLE connection status, RSSI, and packet count |
| `scan` | Force disconnect and restart the BLE scan for nearby gamepads |
| `x`, `o`, `s`, `t` | Tap Cross, Circle, Square, or Triangle |
| `u`, `d`, `l`, `r` | Tap D-Pad Up, Down, Left, or Right |
| `l1`, `r1`, `l2`, `r2`, `l3`, `r3` | Tap shoulder/trigger/stick click buttons |
| `start`, `select` | Tap Start or Select |
| `ps`, `pshold`, `psrel` | Pulse, hold, or release the PS (Home) button |
| `lx`, `ly`, `rx`, `ry <0-255>` | Manually set an analog stick axis position (128 = center) |
| `center` / `reset` | Reset all buttons, triggers, and axes to neutral |
| `demo` | Toggle an automated 10-second test loop on/off |

## Project structure

```text
PAD2PS3/
├── platformio.ini          # PlatformIO board, partition table (huge_app.csv), and build flags
├── CMakeLists.txt          # IDE integration configuration
├── README.md               # Project documentation
└── src/
    ├── main.cpp            # Setup, USB connection lifecycle, Serial CLI
    ├── config_manager.h    # Stick/trigger curve structs, button mapping, NVS & JSON API
    ├── config_manager.cpp  # 2D radial curve math, NVS flash storage, JSON parser/serializer
    ├── web_server.h        # WiFi SoftAP hotspot and HTTP/DNS server interface
    ├── web_server.cpp      # Core 0 web server, REST endpoints, Bluetooth RF coexistence
    ├── web_ui.h            # Embedded responsive Web UI (HTML5 Canvas curve editor & telemetry)
    ├── ble_gamepad.h       # BLE client interface, telemetry & rumble declarations
    ├── ble_gamepad.cpp     # NimBLE host, Core 0 worker task, fast reconnect, rumble watchdog
    ├── usb_hid.h           # DualShock 3 descriptors, handshake structures, API
    ├── usb_hid.cpp         # Direct TinyUSB hardware endpoint streaming & PS3 handshakes
    ├── controller_state.h  # Raw & processed state definitions and button constants
    └── controller_state.cpp # Thread-safe state pipeline, remapping, and report compiler
```

## License

No license has been chosen yet.
