# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ViClean ESP32 Controller is a reverse-engineered BLE controller for ViClean DuschWC (shower toilet) devices. It runs on ESP32 microcontrollers (primarily LilyGO T-Display v1.1) with TFT displays and communicates over Bluetooth Low Energy using the Nordic UART Service (NUS).

The BLE protocol was reverse-engineered from the ViClean Android APK using JADX decompilation. The decompiled Java source is in `decompiled/`.

## Build & Upload

This is an **Arduino IDE project** (no platformio.ini or CMakeLists.txt). There is no CLI build/test/lint pipeline.

- **IDE:** Arduino IDE
- **Board:** LilyGo T-Display (or ESP32 Dev Module)
- **Upload Speed:** 921600
- **Serial Monitor:** 115200 baud
- **Required Libraries:** `TFT_eSPI` by Bodmer (via Library Manager)
- **Board Support URL:** `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`

Each `.ino` sketch directory contains its own `User_Setup.h` for TFT_eSPI display configuration. This file must stay in the same directory as the sketch.

Helper scripts `TFT_ESPI_INSTALL.sh` and `configure_tft_espi.sh` automate library installation and configuration.

## Architecture

### Sketch Variants (in order of complexity)

**`viclean_simple/`** - Recommended starting point. Minimal 2-button UI (~420 lines). Displays W/M program selection on TFT, uses GPIO 35 (SELECT) and GPIO 0 (START/STOP). Hardcoded to Lady/Gent programs with HarmonicWave at level 3-3-3.

**`viclean_simple_battery/`** - Extends `viclean_simple` with ESP32 Deep Sleep (~580 lines). Auto-sleeps after 60s inactivity (~10µA). Display dims 10s before sleep as warning. Both buttons wake from deep sleep. Sleep is disabled while a program is running.

**`viclean_menu/`** - Full settings menu with per-profile configuration (pump, nozzle, temp, harmonic, pulsation) persisted to NVS Flash. Includes deep sleep. Uses a 4-state FSM: `MAIN → RUNNING` or `MAIN → SETTINGS_LIST → SETTINGS_EDIT`. SEL button navigates/increments, GO button confirms/starts/stops.

**`viclean_menuwbatt/`** - Extends `viclean_menu` with battery level display. Reads battery voltage via GPIO 34 (ADC, 1:2 voltage divider) with GPIO 14 as enable pin. Uses eFuse ADC calibration, 20-sample quantile filtering against ESP32 ADC spikes, and a LiPo discharge curve lookup table for accurate percentage. Draws a color-coded battery icon (green >50%, yellow 20-50%, red <20%) in the status bar. Measures every 30s. GPIO 14 is set LOW before deep sleep to save power.

**`viclean_serial_bridge_v2/`** - Full control via Serial Monitor commands (~770 lines). Commands: `start1`-`start5`, `stop`, `temp <1-5>`, `pump <1-6>`, `nozzle <1-6>`, `harmonic`, `puls`, `raw`, etc. Supports 5 predefined programs plus custom control. Display shows command log and responses.

### Supporting Code

- `esp32_viclean_example/` and `lilygo_tdisplay_viclean/` - Alternative/earlier implementations
- `viclean_serial_bridge/` - Original serial bridge, superseded by v2
- `display_test/` - Display-only test (no BLE)

### Python Utilities

- `viclean_command_generator.py` - Command builder library with LRC calculation and all command types
- `viclean_ble_control.py` - Direct laptop BLE control via Bleak (`pip install bleak`). Interactive prompt with auto-discovery.
- `verify_commands.py` - Command checksum verification
- `decode_response.py`, `decode_rx.py` - Device response/RX data decoding
- `simulate_app_start.py` - Simulates ViClean app behavior for testing

### Documentation (mostly German)

- `BLUETOOTH_PROTOCOL_DOCUMENTATION.md` - Comprehensive protocol reference
- `QUICK_REFERENCE.md` - Command lookup table
- `BOARD_IDENTIFICATION.md` - Hardware identification guide

## BLE Protocol

NUS UUIDs:
- Service: `6E400001-B5A3-F393-E0A9-E50E24DCCA9E`
- RX (write): `6E400002-B5A3-F393-E0A9-E50E24DCCA9E`
- TX (notify): `6E400003-B5A3-F393-E0A9-E50E24DCCA9E`

Command structure (Modbus-like):

```
:[ADDRESS][CMD][SUBCMD][DATA][LRC]\r\n
```

- **Address:** `FFFF` (broadcast)
- **CMD:** `0x05` (Write Coil) for most control commands
- **LRC:** XOR checksum of all bytes between `:` and LRC
- **Key subcodes:** `0x8E` START, `0x8F` STOP, `0x85` SET_TEMP, `0x7E` SET_PUMP, `0x81` SET_NOZZLE

### Critical Protocol Constraints

1. **Packet splitting:** Commands >20 bytes (BLE MTU limit) must be split into 20-byte chunks with 50ms delays between them. START commands are 22 bytes and require this splitting. This was the main reverse-engineering breakthrough.

2. **Temperature range is 1-5 only** (not 1-6). The device ignores value 6.

3. **Oscillation and Rotation are coupled** ("HarmonicWave") and can ONLY be set in the START command bitmask. They cannot be toggled individually during a running program.

4. **START command causes BLE disconnect** - this is normal device behavior; it reconnects automatically after ~2 seconds.

### START Command Bitmask Encoding

```cpp
bitmask = ((gender + 1) & 0x03) |        // bits 0-1: gender (0=Lady, 1=Gent)
          ((oscillate & 0x01) << 2) |      // bit 2: oscillation
          ((rotate & 0x01) << 3) |         // bit 3: rotation
          ((pulsate & 0x01) << 4);         // bit 4: pulsation
```

## Key Code Patterns

**Button handling uses ISR flags** - ISRs only set `volatile bool` flags; all display/BLE work happens in `loop()`. Calling `drawUI()` or BLE functions from an ISR causes watchdog crashes.

**GPIO 35 is input-only** on ESP32 - no internal pull-up available. GPIO 0 supports internal pull-up.

**BLE reconnection is flag-based** - a `doConnect` flag is set by the scan callback, and `loop()` handles the actual connection attempt.

**NVS persistence (viclean_menu)** - Uses the ESP32 `Preferences` library to store per-profile settings in Flash. Key format: `profile_W_pump`, `profile_M_temp`, etc. Settings survive deep sleep and power loss.

**Deep sleep (battery variants)** - Uses `esp_deep_sleep_start()` with `ext0`/`ext1` wakeup sources on button GPIOs. Display and BLE are torn down before sleep. Full re-initialization on wake via `setup()`.

**Battery ADC (viclean_menuwbatt)** - GPIO 14 (`ADC_EN`) must be HIGH to power the on-board voltage divider before reading GPIO 34 (`ADC_PIN`). Uses `esp_adc_cal.h` for eFuse Vref calibration. The ESP32 ADC produces positive spikes; the code takes 20 samples, sorts them, and uses the 25th percentile. A 21-point LiPo discharge curve LUT converts voltage (3.27V–4.20V) to percent with linear interpolation. Note: the T-Display v1.1 TP4054 charger CHRG pin is not routed to any GPIO (LED only), so charging status cannot be read directly.

## Hardware

**Primary target:** LilyGO T-Display v1.1 (ESP32 + ST7789 135x240 TFT, Micro-USB). The T-Display S3 variant uses different GPIO pins (GPIO 14 instead of 35 for button).

Display pins: MOSI=19, SCLK=18, CS=5, DC=16, RST=23, Backlight=4.

Battery pins: ADC_EN=14 (voltage divider enable), ADC_PIN=34 (battery voltage via 1:2 divider). No conflict with display or button GPIOs. Note: T-Display S3 uses GPIO 14 for the button, so battery ADC and button share that pin on S3 — the `viclean_menuwbatt` sketch targets v1.1 only.
