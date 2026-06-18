# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ViClean ESP32 Controller is a reverse-engineered BLE controller for ViClean DuschWC (shower toilet) devices. It runs on ESP32 microcontrollers (primarily LilyGO T-Display v1.1) with TFT displays and communicates over Bluetooth Low Energy using the Nordic UART Service (NUS).

The BLE protocol was reverse-engineered from the ViClean Android APK using JADX decompilation.

## Build & Upload

This is an **Arduino IDE project** (no platformio.ini or CMakeLists.txt). There is no CLI build/test/lint pipeline.

- **IDE:** Arduino IDE
- **Board:** LilyGo T-Display (or ESP32 Dev Module)
- **Upload Speed:** 921600
- **Serial Monitor:** 115200 baud
- **Required Libraries:** `TFT_eSPI` by Bodmer (via Library Manager)
- **Board Support URL:** `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`

Each `.ino` sketch directory contains its own `User_Setup.h` for TFT_eSPI display configuration. This file must stay in the same directory as the sketch.

## Architecture

### Two Main Implementations

**`viclean_simple/`** - Recommended starting point. Minimal 2-button UI (~420 lines). Displays W/M program selection on TFT, uses GPIO 35 (SELECT) and GPIO 0 (START/STOP). Hardcoded to Lady/Gent programs with HarmonicWave at level 3-3-3.

**`viclean_serial_bridge_v2/`** - Advanced version (~770 lines). Full control via Serial Monitor commands (`start1`-`start5`, `stop`, `temp <1-5>`, `pump <1-6>`, `nozzle <1-6>`, `harmonic`, `puls`, `raw`, etc.). Supports 5 predefined programs plus custom control.

### Supporting Code

- `esp32_viclean_example/` and `lilygo_tdisplay_viclean/` - Alternative implementations
- `display_test/` - Display-only test (no BLE)
- Python utilities in root: `viclean_command_generator.py` (command builder library), `viclean_ble_control.py` (direct laptop BLE control via Bleak), `verify_commands.py`, `decode_response.py`, `decode_rx.py`

### Documentation (mostly German)

- `BLUETOOTH_PROTOCOL_DOCUMENTATION.md` - Comprehensive protocol reference
- `QUICK_REFERENCE.md` - Command lookup table
- `BOARD_IDENTIFICATION.md` - Hardware identification guide

## BLE Protocol

Uses Nordic UART Service (NUS) with a Modbus-like command structure:

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

## Hardware

**Primary target:** LilyGO T-Display v1.1 (ESP32 + ST7789 135x240 TFT, Micro-USB). The T-Display S3 variant uses different GPIO pins (GPIO 14 instead of 35 for button).

Display pins: MOSI=19, SCLK=18, CS=5, DC=16, RST=23, Backlight=4.
