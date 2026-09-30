# ESP32-S3 PS3 Controller USB HID Emulator

A USB HID game controller emulator firmware running on the ESP32-S3 microcontroller, built with PlatformIO and designed for CLion.

The firmware turns an ESP32-S3 into a USB gamepad that emulates a PlayStation 3 DualShock-compatible controller using the native USB OTG peripheral and TinyUSB stack.

---

## Features

- **Native USB OTG HID**: Runs on the ESP32-S3 native USB D+/D- pins (GPIO 19 / 20) via TinyUSB.
- **PS3 DualShock Identification**: Realistic VID (`0x054C`), PID (`0x0268`), Product Name `"DualShock 3 Compatible Controller"`, and Manufacturer `"Sony Interactive Entertainment"`.
- **Full HID Gamepad Controls**:
  - 16 Digital Buttons (Cross, Circle, Square, Triangle, L1, R1, L2, R2, Select, Start, L3, R3, PS, etc.)
  - 8-Direction D-Pad Hat Switch with centered/neutral state
  - Left Analog Stick (X, Y: 0..255, 128 = Center)
  - Right Analog Stick (Z, Rz: 0..255, 128 = Center)
  - Analog Triggers (L2, R2: 0..255)
- **Automated Controller Test Mode**: 10-second repeating loop exercising all sticks, buttons, and triggers.
- **Bi-directional Communication & PS3 Extension Hooks**:
  - Receives and logs Host OUT reports for rumble motors (weak/strong) and Player 1–4 LEDs.
  - Responds to Feature Report `0xF2` (queried by PS3 / Linux `hid-sony` driver for Bluetooth MAC pairing).
  - Includes full 49-byte native PS3 report structure for Phase 2 console pairing.
- **Serial Debugging over Native USB**: Composite CDC ACM serial interface allows reading live debug logs and flashing over the same native USB port.

---

## Project Structure

```
PS3CTRL/
├── platformio.ini         # PlatformIO project configuration
├── CMakeLists.txt         # CLion integration targets (Build, Flash, Monitor)
├── .gitignore             # Git ignore file
└── src/
    ├── main.cpp           # Setup, automated test sequence state machine, loop
    ├── usb_hid.h          # USB descriptor, VID/PID, callbacks, PS3 data structures
    ├── usb_hid.cpp        # TinyUSB stack initialization and USBHIDDevice subclass
    ├── controller_state.h # ControllerState struct and state management declarations
    └── controller_state.cpp # State storage, setButton(), setAxis(), sendReport()
```

---

## Automated Test Sequence

On boot, the firmware initializes USB HID and begins executing a continuous 10-second test cycle:

| Time | Phase | Description |
|---|---|---|
| **0.0s – 2.0s** | Left Stick | Moves slowly: Center → Full Left → Full Right → Center → Full Up → Full Down → Center |
| **2.0s – 5.9s** | Buttons | Sequentially presses each of the 13 buttons for 300ms: Cross, Circle, Square, Triangle, L1, R1, L2, R2, Select, Start, L3, R3, PS |
| **5.9s – 10.0s**| Right Stick & Triggers | Sweeps the right analog stick in a continuous circular pattern, while sweeping L2 and R2 triggers from 0 to 255 (triangle wave) and rotating the D-pad |

---

## Hardware Connection Guide

> [!IMPORTANT]
> The ESP32-S3 has **two USB ports** or USB modes:
> 1. **USB-to-UART / Debug Port** (Connected to CP2102/CH340 or ESP32-S3 internal USB-Serial/JTAG).
> 2. **Native USB OTG Port** (Directly connected to ESP32-S3 GPIO 19 `D-` and GPIO 20 `D+`).

### ESP32-S3 Boards with Two Type-C Ports (DevKitC-1 / Freenove / etc.)
- Connect your USB cable to the port labeled **`USB`** or **`OTG`** (NOT the one labeled `UART`).
- The native USB port will enumerate as both the **Game Controller** and the **CDC Serial Port**.

### ESP32-S3 Boards with Single USB Port or Breakout Headers
- Ensure the port is wired to **GPIO 19 (`D-`)** and **GPIO 20 (`D+`)**.
- If your board has a single port routed through an external UART bridge chip (CP2102), USB HID will NOT work through that chip. You must solder a USB breakout connector directly to GPIO 19, GPIO 20, 5V, and GND.

---

## Building and Flashing

### Using PlatformIO CLI

1. **Build firmware**:
   ```bash
   pio run
   ```

2. **Flash to ESP32-S3**:
   ```bash
   pio run -t upload
   ```
   *(If flashing for the first time or if the board is in bootloader loop, hold the **BOOT** button, press and release **RESET**, then release **BOOT**).*

3. **Open Serial Monitor**:
   ```bash
   pio device monitor
   ```

### Using CLion

1. Open CLion and choose **File → Open**, selecting `/home/weexnes/PS3CTRL`.
2. CLion's PlatformIO plugin / CMake integration provides toolbar run configurations:
   - `PLATFORMIO_BUILD` (Compile)
   - `PLATFORMIO_UPLOAD` (Flash)
   - `PLATFORMIO_MONITOR` (Serial Monitor)
3. Hit the **Run** button on `PLATFORMIO_UPLOAD`.

---

## Verifying the Game Controller

### 1. Web Browser Gamepad Tester
1. Open Chrome or Chromium.
2. Navigate to [hardwaretester.com/gamepad](https://hardwaretester.com/gamepad).
3. The tester will display **"DualShock 3 Compatible Controller"** or **"Sony Interactive Entertainment (Vendor: 054c Product: 0268)"**.
4. Observe the automated test sequence:
   - The left thumbstick sweeps X and Y.
   - Buttons 0 to 12 light up one by one.
   - The right thumbstick rotates in a circle and triggers B6/B7 (L2/R2) sweep smoothly.

### 2. Linux Command Line
- **`lsusb`**: Look for:
  ```
  Bus 001 Device 012: ID 054c:0268 Sony Corp. Batoh Device / PlayStation 3 Controller
  ```
- **`evtest`** or **`jstest-gtk`**:
  ```bash
  sudo evtest
  ```
  Select the controller from the list to view real-time button and axis events.

### 3. Windows
- Press `Win + R`, type `joy.cpl`, and press Enter.
- Click **Properties** to see the 16 buttons and dual analog sticks test dialog.

---

## Troubleshooting

1. **Controller not detected by PC**:
   - Check which USB port on the ESP32-S3 is plugged in. It must be the **USB / OTG** port (GPIO 19/20), NOT the UART port.
   - Verify the USB cable supports data transfer (many charging-only cables omit D+/D- wires).

2. **Linux loads `hid-sony` driver and stalls**:
   - If your Linux kernel's `hid-sony` module strictly expects Sony's private proprietary Bluetooth handshake, you can change the VID/PID in [src/usb_hid.h](file:///home/weexnes/PS3CTRL/src/usb_hid.h) to a generic PC gamepad:
     ```cpp
     #define USB_CONTROLLER_VID  0x045E // Microsoft
     #define USB_CONTROLLER_PID  0x028E // Xbox 360 Controller
     ```
     or
     ```cpp
     #define USB_CONTROLLER_VID  0x0079 // DragonRise / Generic Gamepad
     #define USB_CONTROLLER_PID  0x0006
     ```

3. **Failed to upload / No serial port found**:
   - Put ESP32-S3 into ROM Bootloader mode manually:
     1. Hold down the **BOOT** button (GPIO 0).
     2. Press the **RST** button.
     3. Release **RST**, then release **BOOT**.
     4. Run `pio run -t upload`.
