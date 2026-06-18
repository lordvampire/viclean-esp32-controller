# ViClean ESP32 Controller

ESP32-based controller for ViClean DuschWC (shower toilet) via Bluetooth LE.

## 🎯 Project Overview

This project reverse-engineers the ViClean Bluetooth protocol and implements an ESP32-based controller with a simple 2-button UI on a LilyGO T-Display.

### Features

✅ Full BLE communication with ViClean devices
✅ START/STOP programs
✅ Adjust temperature, water pressure, nozzle position
✅ Control oscillation, rotation, pulsation
✅ Simple 2-button UI with TFT display
✅ Automatic BLE reconnection
✅ Packet splitting for long commands (>20 bytes)

## 📦 Projects

### 1. ViClean Simple (`viclean_simple/`)

**Minimal UI with 2 programs - RECOMMENDED FOR BEGINNERS**

- **Display:** Large W/M letters with selection indicator
- **Controls:** 2 buttons only (SELECT + START/STOP)
- **Programs:**
  - W (Woman/Lady): Lady mode with HarmonicWave
  - M (Men/Gent): Gent mode with HarmonicWave
- **Code:** ~300 lines, clean and simple

[📖 Documentation](viclean_simple/README.md)

### 2. ViClean Simple Battery (`viclean_simple_battery/`)

**Batterieschonende Version mit Deep Sleep**

- **Basis:** viclean_simple + automatischer Deep Sleep
- **Akku:** 450mAh LiPo am T-Display JST-Anschluss
- **Sleep:** Nach 60 Sekunden Inaktivität → Deep Sleep (~10µA)
- **Wake-Up:** Beliebiger Button drückt → ESP32 startet neu
- **Display dimmt** 10 Sekunden vor Sleep als Warnung

[📖 Documentation](viclean_simple_battery/README.md)

### 3. ViClean Serial Bridge V2 (`viclean_serial_bridge_v2/`)

**Advanced version with full control via Serial Monitor**

- **Interface:** Serial commands (115200 baud)
- **Features:** All commands, test modes, raw commands
- **Programs:** 5 predefined + custom
- **Code:** ~800 lines with debugging features

## 🔧 Hardware Requirements

- **LilyGO T-Display v1.1** (ESP32 + 135x240 TFT)
  - Or any ESP32 with ST7789 display
- **ViClean DuschWC** with Bluetooth

### T-Display Pin Configuration

```
GPIO 35 - Button LEFT (SELECT)
GPIO 0  - Button RIGHT (START/STOP)
GPIO 4  - TFT Backlight
GPIO 5  - TFT CS
GPIO 16 - TFT DC
GPIO 18 - TFT SCLK
GPIO 19 - TFT MOSI
GPIO 23 - TFT RST
```

## 🚀 Quick Start

### 1. Install Arduino IDE

Download from [arduino.cc](https://www.arduino.cc/en/software)

### 2. Install ESP32 Board Support

**Arduino IDE → Preferences → Additional Boards Manager URLs:**
```
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```

**Tools → Board → Boards Manager → Search "esp32" → Install**

### 3. Install Libraries

**Sketch → Include Library → Manage Libraries:**

- `TFT_eSPI` by Bodmer

### 4. Upload Code

1. Open `viclean_simple/viclean_simple.ino`
2. Select **Board:** "LilyGo T-Display"
3. Select **Port:** Your ESP32 port
4. Click **Upload**

### 5. Use It!

- Left button: Select W or M
- Right button: START / STOP

## 📚 Protocol Documentation

### BLE Service & Characteristics

```
Service UUID:  6E400001-B5A3-F393-E0A9-E50E24DCCA9E (Nordic UART Service)
RX UUID:       6E400002-B5A3-F393-E0A9-E50E24DCCA9E (write to device)
TX UUID:       6E400003-B5A3-F393-E0A9-E50E24DCCA9E (notifications from device)
```

### Command Format

```
:FFFF[CMD][SUBCMD][DATA][LRC]\r\n

Example (START):
:FFFF058E0D0303030F\r\n
     │││││││││││└─ LRC checksum
     ││││││││└─── Temp (3)
     │││││││└──── Nozzle (3)
     ││││││└───── Pump (3)
     │││││└────── Bitmask (0x0D)
     ││││└─────── Subcmd (0x8E = START)
     │││└──────── Cmd (0x05 = Write Coil)
     ││└───────── Address (FFFF = Broadcast)
     │└────────── Start marker
     └─────────── End: \r\n
```

### Important Commands

| Command | Code | Description |
|---------|------|-------------|
| START_PROGRAM | 0x8E | Start shower program |
| BREAK_PROGRAM | 0x8F | Stop program |
| SET_TEMP | 0x85 | Temperature (1-5) |
| SET_WATER_PUMP | 0x7E | Pressure (1-6) |
| SET_NOZZLE_POSITION | 0x81 | Nozzle position (1-6) |
| TOGGLE_PULSATING | 0x8D | Pulsation on/off |
| TOGGLE_HW | 0x8A | Hot water on/off |
| TOGGLE_BUZZER | 0x65 | Buzzer on/off |

[📖 Full Protocol Documentation](BLUETOOTH_PROTOCOL_DOCUMENTATION.md)

## 🔍 Key Findings from Reverse Engineering

### Critical Discoveries

1. **Packet Splitting Required**
   - Commands >20 bytes MUST be split into 20-byte packets
   - This was the main issue preventing START commands from working

2. **Value Ranges**
   - Temperature: 1-5 (NOT 1-6!)
   - Pump: 1-6
   - Nozzle: 1-6
   - Values are 1-indexed (app adds +1 to slider values 0-4)

3. **Oscillation & Rotation**
   - OSC and ROT are coupled ("HarmonicWave")
   - Can ONLY be set during START (in bitmask)
   - Cannot be toggled individually during program

4. **BLE Behavior**
   - START command disconnects BLE (normal behavior!)
   - Device reconnects automatically
   - Command queue with 100ms delays between commands

### Bitmask Encoding (START command)

```cpp
bitmask = ((gender + 1) & 0x03) |
          ((oscillate & 0x01) << 2) |
          ((rotate & 0x01) << 3) |
          ((pulsate & 0x01) << 4);

Example:
Lady + HarmonicWave = 0x0D = 00001101
  Bit 0-1: 01 (Lady+1)
  Bit 2:   1  (Oscillate ON)
  Bit 3:   1  (Rotate ON)
  Bit 4:   0  (Pulsate OFF)
```

## 📁 Repository Structure

```
ViClean/
├── viclean_simple/              # Simple 2-button UI (RECOMMENDED)
│   ├── viclean_simple.ino       # Main code
│   ├── User_Setup.h             # TFT config
│   └── README.md                # Documentation
│
├── viclean_simple_battery/      # Battery version with Deep Sleep
│   ├── viclean_simple_battery.ino
│   ├── User_Setup.h
│   └── README.md
│
├── viclean_serial_bridge_v2/    # Advanced serial control
│   ├── viclean_serial_bridge_v2.ino
│   ├── User_Setup.h
│   └── README.md
│
├── BLUETOOTH_PROTOCOL_DOCUMENTATION.md  # Full protocol docs
├── QUICK_REFERENCE.md                   # Command quick reference
├── FINALE_ZUSAMMENFASSUNG.md           # Final summary (German)
│
└── Python Tools (for analysis)
    ├── verify_commands.py       # Verify command checksums
    ├── decode_rx.py             # Decode device responses
    └── simulate_app_start.py    # Simulate app behavior
```

## 🎓 Reverse Engineering Process

This project was created by:

1. **APK Decompilation** using JADX
2. **BLE Protocol Analysis** from Java source code
3. **Command Structure Mapping** (Modbus-like protocol)
4. **Systematic Testing** to validate findings
5. **ESP32 Implementation** with packet splitting

Key files analyzed:
- `VicBluetoothManager.java` - BLE communication
- `VicCommand.java` - Command encoding
- `MainActivity.java` - UI logic and value ranges

## ⚠️ Important Notes

### Limitations

- Temperature has only 5 levels (not 6!)
- Oscillation/Rotation cannot be changed during program
- START command causes brief BLE disconnect (normal)

### Safety

- Only for private use
- Test thoroughly before regular use
- Original app remains functional
- No warranty provided

## 🛠️ Troubleshooting

### Display stays black
- Check backlight pin (GPIO 4)
- Verify `User_Setup.h` is in project folder
- Ensure TFT_BL is HIGH

### Buttons don't work
- GPIO 35 is input-only (no pull-up!)
- GPIO 0 has internal pull-up
- Check for watchdog timeouts (don't call `drawUI()` in ISR!)

### No BLE connection
- ViClean must be in range
- Check Serial Monitor (115200 baud)
- Wait for "ViClean gefunden" message

### Program doesn't start
- Wait for "CONN" indicator
- BLE disconnect after START is normal
- Device reconnects automatically

## 📄 License

This project is for educational and personal use only.

Reverse engineering was performed on the ViClean mobile app for interoperability purposes.

**No affiliation with or endorsement by ViClean/Geberit.**

## 🙏 Acknowledgments

- **JADX** - Java decompiler
- **Nordic Semiconductor** - NUS (Nordic UART Service) specification
- **Bodmer** - TFT_eSPI library
- **LilyGO** - T-Display hardware

## 📞 Support

For questions or issues, please open a GitHub issue.

---

**Status:** ✅ Fully functional - All core features working!

**Last Updated:** February 2026
