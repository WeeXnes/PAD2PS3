# ESP32-S3 DualShock 3 Emulator & Wireless BLE Controller Bridge

A high-performance, low-latency firmware for the **ESP32-S3** microcontroller that emulates an authentic **Sony PlayStation 3 DualShock 3** controller over native USB OTG while connecting wirelessly to Bluetooth Low Energy gamepads—specifically tailored for the **Google Stadia Controller** (Bluetooth Mode).

Turn your Google Stadia controller (or other BLE gamepads) into a native, fully functional PS3 controller with full button mapping, smooth analog sticks, analog triggers, interactive Serial CLI, and **bidirectional rumble/haptic feedback**.

---

## Features

- **Authentic Sony DualShock 3 USB Emulation**:
  - Emulates an authentic Sony DualShock 3 (`054C:0268`) on the ESP32-S3 native USB OTG peripheral (GPIO 19 `D-` / GPIO 20 `D+`).
  - Implements genuine PS3 console handshake: responds to proprietary Feature Reports (`0x01`, `0xF2`, `0xF5`, `0xEF`, `0xF8`, `0xF7`, and `0xF4` streaming activation).
  - Automatically pulses the PS button upon handshake completion to assign controller player number (`Player 1..4` LED detection).
  - Streams standard 48-byte native DualShock 3 reports at **100Hz (10ms)**.

- **Wireless BLE Gamepad Host (Google Stadia Controller)**:
  - Scans, connects, and bonds automatically to BLE HID gamepads using NimBLE.
  - Tailored mapping for Google Stadia Bluetooth mode:
    - **D-Pad**: 8-direction Hat Switch (Up, Down, Left, Right, Diagonals)
    - **Action Buttons**: $\text{A} \rightarrow \text{Cross}$, $\text{B} \rightarrow \text{Circle}$, $\text{X} \rightarrow \text{Square}$, $\text{Y} \rightarrow \text{Triangle}$
    - **Bumpers & Clicks**: L1, R1 bumpers; L3, R3 analog stick clicks
    - **Triggers**: Smooth analog L2 and R2 (`0..255`) plus digital click thresholds
    - **System Buttons**: Options ($\dots$) $\rightarrow$ Select, Menu ($\equiv$) $\rightarrow$ Start, Stadia Logo $\rightarrow$ PS / Home button, Assistant & Capture buttons

- **Bidirectional Rumble / Haptic Feedback**:
  - Intercepts PS3 host Output Report `0x01` motor commands in real time.
  - Maps PS3 dual motors (low-frequency heavy motor and high-frequency light buzz) to Stadia 16-bit PID Haptic Output Reports (Report ID 5).
  - **Calibrated Vibration Profile**: Small motor is scaled to a realistic duty cycle (~36%) instead of violent 100% saturation.
  - **Live Intensity Control**: Master rumble strength is adjustable on the fly via serial CLI (`intensity <0-100>`), defaulting to a comfortable 60%.

- **Interactive Serial CLI**:
  - Operates over the board's hardware UART port at **115200 baud** without interfering with USB OTG.
  - Allows manual button triggering, axis positioning, live input logging (`log`), rumble testing (`rumble`), intensity adjustment (`intensity`), and BLE status checks (`ble`, `scan`).

- **Zero-Latency Design**:
  - Verbose library logging compiled out for maximum throughput.
  - Rate-limited, non-blocking rumble writes to preserve tight 100Hz controller polling rates without USB bus drops.

---

## Hardware Setup

```
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
> 1. **`USB` / `OTG` Port**: Connects directly to the **PlayStation 3** console (or PC).
> 2. **`UART` Port**: Connects to your computer for PlatformIO flashing and the interactive Serial CLI monitor.

---

## Quick Start

### 1. Build and Flash Firmware

Connect your PC to the ESP32-S3 **`UART`** port:

```bash
# Build the project
pio run

# Flash to the ESP32-S3
pio run -t upload

# Open the serial monitor
pio device monitor
```

### 2. Put Google Stadia Controller into Bluetooth Mode

If you haven't already unlocked Bluetooth on your Stadia controller, visit [Google Stadia Bluetooth Unlock](https://stadia.google.com/controller/) using Chrome.

1. Ensure the controller is powered off.
2. Hold the **Stadia button** and **`Y`** together for 3 seconds until the status LED pulses **orange**.
3. The ESP32-S3 will automatically detect `Stadia...`, pair, bond, and subscribe to input reports.
4. The controller's LED will turn **solid white** once paired.

### 3. Connect to the PS3 Console

1. Connect the ESP32-S3 **`USB` (OTG)** port to the front USB port of your PS3 using a high-quality data USB cable.
2. The ESP32-S3 will perform the authentic Sony handshake, send the PS button pulse, and pair with the console.
3. The PS3 will assign Player 1 (or next available slot), and the Stadia controller will immediately control the PS3 console!

---

## Interactive Serial CLI Reference

You can send commands directly to the ESP32-S3 over the serial monitor (`115200` baud, newline terminated):

| Command | Description |
|---|---|
| `help` / `?` | Print list of all available commands |
| `status` | Print current controller input state (buttons, axes, d-pad) |
| `log` / `debug` | Toggle live real-time input logging on/off |
| `intensity <0-100>` | Set master rumble intensity percentage (e.g. `intensity 60`) |
| `intensity` | Print current master rumble intensity percentage |
| `rumble [0-255]` | Trigger a 500ms test rumble pulse on the connected gamepad |
| `ble` | Print current Bluetooth / BLE connection status and peer MAC |
| `scan` | Force restart BLE scan for nearby gamepads |
| `x` / `cross` | Tap Cross button |
| `o` / `circle` | Tap Circle button |
| `s` / `square` | Tap Square button |
| `t` / `triangle` | Tap Triangle button |
| `u`, `d`, `l`, `r` | Tap D-Pad Up, Down, Left, or Right |
| `l1`, `r1` | Tap L1 or R1 bumper |
| `l2`, `r2` | Tap L2 or R2 trigger |
| `l3`, `r3` | Tap L3 or R3 stick click |
| `start`, `select` | Tap Start or Select button |
| `ps` | Pulse PS (Home) button for 200ms |
| `pshold` | Hold down PS button |
| `psrel` | Release PS button |
| `lx <0-255>` | Manually set Left Stick X axis (128 = center) |
| `ly <0-255>` | Manually set Left Stick Y axis (128 = center) |
| `rx <0-255>` | Manually set Right Stick X axis (128 = center) |
| `ry <0-255>` | Manually set Right Stick Y axis (128 = center) |
| `center` / `neutral` | Reset all buttons, triggers, and axes to neutral |
| `demo` | Toggle automated 10-second test loop on/off |

---

## Project Structure

```
PS3CTRL/
├── platformio.ini          # PlatformIO board, clock, and build flags
├── CMakeLists.txt          # IDE integration configuration
├── README.md               # Project documentation
└── src/
    ├── main.cpp            # Setup, USB connection lifecycle, Serial CLI
    ├── ble_gamepad.h       # BLE client interface, rumble intensity declarations
    ├── ble_gamepad.cpp     # NimBLE host, Stadia mapping, haptic translation
    ├── usb_hid.h           # DualShock 3 descriptors, handshake structures, API
    ├── usb_hid.cpp         # TinyUSB stack implementation, authentic PS3 responses
    ├── controller_state.h  # State storage definitions and button constants
    └── controller_state.cpp # Active controller state buffer and report compiler
```

---

## Technical Details

### PS3 USB Handshake Sequence
Authentic PS3 consoles require a specific USB feature exchange before accepting controller inputs:
1. `GET_REPORT (Feature 0x01)`: Controller capabilities.
2. `GET_REPORT (Feature 0xF2)`: DualShock 3 Bluetooth MAC address inquiry.
3. `GET_REPORT / SET_REPORT (Feature 0xF5)`: PS3 Host Bluetooth MAC exchange.
4. `SET_REPORT (Feature 0xEF)`: Encryption/subcmd handshake (`0xA0` and `0xB0`).
5. `GET_REPORT (Feature 0xF8)` and `(Feature 0xF7)`: Final handshake verification.
6. **PS Button Pulse**: Firmware pulses the PS button to register player assignment.
7. `SET_REPORT (Feature 0xF4)`: PS3 activates streaming (`0x42 0x0C`) and assigns Player LED.

### Rumble / Haptic Mapping
- **PS3 DualShock 3 Output Report `0x01`**:
  - `buffer[1..2]`: Right motor duration and power (binary high-frequency motor).
  - `buffer[3..4]`: Left motor duration and power (analog low-frequency motor, 0..255).
- **Google Stadia PID Output Report `0x05`**:
  - Stadia expects 4 bytes: `[left_lo, left_hi, right_lo, right_hi]` in 16-bit range (`0..65534`).
  - The firmware translates the PS3 command into appropriate 16-bit actuator values and applies master intensity scaling with a 40ms rate limit to prevent BLE buffer congestion.

---

## License

This project is open-source and intended for personal and educational use. PlayStation and DualShock are trademarks of Sony Interactive Entertainment Inc. Google Stadia is a trademark of Google LLC. This project is not affiliated with or endorsed by Sony or Google.
