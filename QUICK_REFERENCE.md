# ViClean DuschWC - Quick Reference

## BLE Connection Info
```
Service UUID:  6E400001-B5A3-F393-E0A9-E50E24DCCA9E
RX (Write):    6E400002-B5A3-F393-E0A9-E50E24DCCA9E
TX (Notify):   6E400003-B5A3-F393-E0A9-E50E24DCCA9E
```

## Command Format
```
:[ADRESSE][CMD][SUBCMD][DATA][LRC]\r\n
  │    │      │     │      │    │
  │    │      │     │      │    └─ XOR Checksum (ASCII)
  │    │      │     │      └────── Parameters (ASCII-Hex)
  │    │      │     └───────────── Subcommand (ASCII-Hex)
  │    │      └─────────────────── Modbus Command (ASCII-Hex)
  │    └────────────────────────── Always "FFFF"
  └─────────────────────────────── Start Byte (0x3A = ':')
```

## Modbus Commands
| Code | Name | Usage |
|------|------|-------|
| 0x03 | Read Register | Read values |
| 0x05 | Write Coil | Write On/Off commands |
| 0x06 | Write Register | Write register values |

## Command Codes
| Command | Hex | Example |
|---------|-----|---------|
| **Program Control** | | |
| Start Program | 0x8E | `:FFFF058E150303037F\r\n` |
| Break Program | 0x8F | `:FFFF058F7B\r\n` |
| **Settings** | | |
| Set Temperature | 0x85 | `:FFFF0585040C\r\n` (Level 4) |
| Set Water Pump | 0x7E | `:FFFF057E0275\r\n` (Level 2) |
| Set Nozzle Position | 0x81 | `:FFFF0581030F\r\n` (Level 3) |
| **Toggles** | | |
| Toggle Oscillation | 0x88 | `:FFFF058805\r\n` |
| Toggle Rotation | 0x89 | `:FFFF058904\r\n` |
| Toggle Pulsating | 0x8D | `:FFFF058D79\r\n` |
| Toggle Hot Water | 0x8A | `:FFFF058A7C\r\n` |
| Toggle Buzzer | 0x65 | `:FFFF056502\r\n` |
| **Maintenance** | | |
| Nozzle Maintenance | 0x82 | `:FFFF058201000E\r\n` |

## Start Program Parameters
```
Byte 0: Bitmask
  Bit 0-1: Gender (0=Lady, 1=Gent)
  Bit 2:   Oscillate (0=Off, 1=On)
  Bit 3:   Rotate (0=Off, 1=On)
  Bit 4:   Pulsate (0=Off, 1=On)

Byte 1: Pump State (0-5)
Byte 2: Nozzle State (0-5)
Byte 3: Temperature (0-5)
```

### Bitmask Examples
| Mode | Gender | Osc | Rot | Puls | Bitmask |
|------|--------|-----|-----|------|---------|
| Lady Basic | 0 | 0 | 0 | 0 | 0x01 |
| Lady + Osc | 0 | 1 | 0 | 0 | 0x05 |
| Lady + Osc + Puls | 0 | 1 | 0 | 1 | 0x15 |
| Gent Basic | 1 | 0 | 0 | 0 | 0x02 |
| Gent + Rot | 1 | 0 | 1 | 0 | 0x0A |
| All Features | 0 | 1 | 1 | 1 | 0x1D |

## ESP32 Quick Start

### 1. Include Libraries
```cpp
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
```

### 2. Define UUIDs
```cpp
#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
```

### 3. Send Command
```cpp
String cmd = ":FFFF0585040C\r\n";
pCharacteristicRX->writeValue(cmd.c_str(), cmd.length());
```

## Python Quick Test

### Generate Command
```python
from viclean_command_generator import *

# Set temperature to level 4
cmd = cmd_set_temperature(4)
print(cmd)  # :FFFF0585040C\r\n

# Start program (Lady, Oscillation)
cmd = create_start_program_command(
    gender=0, oscillate=1, rotate=0, pulsate=0,
    pumpstate=3, nozzlestate=3, temperature=3
)
print(cmd)  # :FFFF058E050303037D\r\n
```

## LRC Calculation

### Python
```python
def calculate_lrc(data_bytes):
    lrc = 0
    for byte in data_bytes:
        lrc ^= byte
    return lrc
```

### C++
```cpp
uint8_t calculateLRC(const char* data, size_t length) {
    uint8_t lrc = 0;
    for (size_t i = 0; i < length; i++) {
        lrc ^= (uint8_t)data[i];
    }
    return lrc;
}
```

## Testing Checklist

- [ ] BLE connection established
- [ ] Services discovered
- [ ] Characteristics found
- [ ] Notifications enabled
- [ ] Simple command works (e.g., Toggle Buzzer)
- [ ] Parameter command works (e.g., Set Temperature)
- [ ] Complex command works (e.g., Start Program)
- [ ] LRC calculation correct
- [ ] Device responds to commands

## Common Issues

| Problem | Solution |
|---------|----------|
| No connection | Check if device is in pairing mode |
| Commands not working | Verify LRC checksum |
| Data not received | Enable notifications on TX |
| Disconnects frequently | Reduce command frequency (100ms delay) |
| Wrong parameters | Check value ranges (0-5) |

## Value Ranges

| Parameter | Min | Max | Description |
|-----------|-----|-----|-------------|
| Temperature | 0 | 5 | Cold to Hot |
| Water Pump | 0 | 5 | Low to High pressure |
| Nozzle Position | 0 | 5 | Front to Back |
| Gender | 0 | 1 | Lady / Gent |
| Toggle Flags | 0 | 1 | Off / On |

---

For detailed information, see: `BLUETOOTH_PROTOCOL_DOCUMENTATION.md`
