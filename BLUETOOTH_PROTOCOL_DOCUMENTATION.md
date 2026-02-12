# ViClean DuschWC Bluetooth-Protokoll Dokumentation

## Zusammenfassung
Diese Dokumentation beschreibt das Bluetooth Low Energy (BLE) Kommunikationsprotokoll der ViClean DuschWC App.
Das Protokoll basiert auf dem Nordic UART Service (NUS) und verwendet ein Modbus-ähnliches Befehlsformat.

---

## 1. BLE Service und Characteristics

### Nordic UART Service (NUS)
Das ViClean DuschWC verwendet den standardisierten Nordic UART Service:

| Service/Characteristic | UUID | Beschreibung |
|------------------------|------|--------------|
| **NUS Service** | `6E400001-B5A3-F393-E0A9-E50E24DCCA9E` | Haupt-Service für UART-Kommunikation |
| **TX Characteristic** | `6E400003-B5A3-F393-E0A9-E50E24DCCA9E` | Daten vom Gerät empfangen (Notify) |
| **RX Characteristic** | `6E400002-B5A3-F393-E0A9-E50E24DCCA9E` | Daten zum Gerät senden (Write) |
| **CCCD** | `00002902-0000-1000-8000-00805f9b34fb` | Client Characteristic Configuration Descriptor |

### Kommunikationsrichtung
- **RX (0x6E400002)**: App → DuschWC (Befehle senden)
- **TX (0x6E400003)**: DuschWC → App (Benachrichtigungen empfangen)

---

## 2. Befehlsprotokoll-Format

### Basis-Struktur
Alle Befehle folgen diesem Format:
```
:[ADRESSE][CMD][SUBCMD][DATA][LRC]\r\n
```

### Detaillierte Beschreibung

| Feld | Bytes | Hex-Wert | Beschreibung |
|------|-------|----------|--------------|
| **STX** | 1 | `0x3A` (`:`) | Start of Text - Markiert Befehlsbeginn |
| **ADRESSE** | 4 (ASCII) | `FFFF` | Geräteadresse (immer 0xFF 0xFF in ASCII: "FFFF") |
| **CMD** | 2 (ASCII) | variabel | Modbus-Befehlscode (siehe unten) |
| **SUBCMD** | 2 (ASCII) | variabel | Subbefehl-Code |
| **DATA** | variabel | variabel | Parameterdaten (ASCII-codiert) |
| **LRC** | 2 (ASCII) | variabel | Longitudinal Redundancy Check (XOR-Checksum) |
| **CRLF** | 2 | `0x0D 0x0A` | Carriage Return + Line Feed |

### Modbus-Befehle
| Befehlscode | Beschreibung | Verwendung |
|-------------|--------------|------------|
| `0x03` (3) | Read Register | Register-Werte lesen |
| `0x05` (5) | Write Coil | Einzelne Coils schreiben (On/Off) |
| `0x06` (6) | Write Register | Register-Werte schreiben |

### LRC-Checksum Berechnung
Die LRC (Longitudinal Redundancy Check) wird durch XOR aller Bytes berechnet:
```c
byte calculateLRC(byte[] bytes) {
    byte LRC = 0;
    for (byte b : bytes) {
        LRC = b ^ LRC;
    }
    return LRC;
}
```

### Byte-zu-ASCII-Konvertierung
Alle Daten werden als ASCII-Hex-Strings codiert:
- Beispiel: Byte `0x8E` wird zu ASCII-String `"8E"` → `0x38 0x45`

---

## 3. Befehlsübersicht

### Verfügbare Befehle (Subcommands)

| Befehl | Hex-Code | Dezimal | Beschreibung |
|--------|----------|---------|--------------|
| `BT_COMMAND_START_CW_NOZZLE` | `0x70` | 112 | Düse im Uhrzeigersinn bewegen |
| `BT_COMMAND_START_CCW_NOZZLE` | `0x71` | 113 | Düse gegen Uhrzeigersinn bewegen |
| `BT_COMMAND_START_CW_WIGGLE` | `0x72` | 114 | Wackelbewegung im Uhrzeigersinn |
| `BT_COMMAND_START_PROGRAM` | `0x8E` | -114 (142) | Programm starten |
| `BT_COMMAND_BREAK_PROGRAM` | `0x8F` | -113 (143) | Programm abbrechen |
| `BT_COMMAND_SET_NOZZLE_POSITION` | `0x81` | -127 (129) | Düsenposition setzen |
| `BT_COMMAND_SET_TEMP` | `0x85` | -123 (133) | Temperatur setzen |
| `BT_COMMAND_SET_WATER_PUMP` | `0x7E` | 126 | Wasserpumpendruck setzen |
| `BT_COMMAND_TOGGLE_OSCILLATION` | `0x88` | -120 (136) | Oszillation umschalten |
| `BT_COMMAND_TOGGLE_ROTATION` | `0x89` | -119 (137) | Rotation umschalten |
| `BT_COMMAND_TOGGLE_PUSLATING` | `0x8D` | -115 (141) | Pulsation umschalten |
| `BT_COMMAND_TOGGLE_HW` | `0x8A` | -118 (138) | Warmwasser umschalten |
| `BT_COMMAND_TOGGLE_BUZZER` | `0x65` | 101 | Buzzer umschalten |
| `BT_COMMAND_NOZZLE_MAINTANNCE` | `0x82` | -130 (130) | Düsen-Wartungsmodus |

---

## 4. Detaillierte Befehlsbeschreibungen

### 4.1 Programm starten (START_PROGRAM)
Der komplexeste Befehl - startet ein Waschprogramm mit allen Parametern.

**Befehlscode:** `0x8E` (142)
**Modbus-Befehl:** Write Coil (0x05)

**Parameter-Struktur:**
```
Byte 0: Bitmask-Parameter
  Bits 0-1: Gender (0=Lady, 1=Gent, 2-3=Reserved)
  Bit 2: Oscillate (0=Off, 1=On)
  Bit 3: Rotate (0=Off, 1=On)
  Bit 4: Pulsate (0=Off, 1=On)
  Bits 5-7: Reserved

Byte 1: Pumpstate (Wasserdruck: 0-5)
Byte 2: Nozzlestate (Düsenposition: 0-5)
Byte 3: Temperature (Temperatur: 0-5)
```

**Bitmask-Berechnung:**
```c
byte bitmask = ((gender + 1) & 0x03) |
               ((oscillate & 0x01) << 2) |
               ((rotate & 0x01) << 3) |
               ((pulsate & 0x01) << 4);
```

**Beispiel:**
- Gender: Lady (0)
- Oscillate: On (1)
- Rotate: Off (0)
- Pulsate: On (1)
- Pumpstate: 3
- Nozzlestate: 3
- Temperature: 3

Ergibt Bitmask: `0x15` (00010101)

### 4.2 Programm abbrechen (BREAK_PROGRAM)
**Befehlscode:** `0x8F` (143)
**Parameter:** Keine

### 4.3 Düsenposition setzen (SET_NOZZLE_POSITION)
**Befehlscode:** `0x81` (129)
**Parameter:** 1 Byte (Position: 0-5)

### 4.4 Temperatur setzen (SET_TEMP)
**Befehlscode:** `0x85` (133)
**Parameter:** 1 Byte (Temperatur: 0-5)
- 0 = Kalt
- 1-5 = Warm (zunehmend)

### 4.5 Wasserpumpe setzen (SET_WATER_PUMP)
**Befehlscode:** `0x7E` (126)
**Parameter:** 1 Byte (Druck: 0-5)

### 4.6 Toggle-Befehle
Diese Befehle schalten Features ein/aus ohne Parameter:
- **TOGGLE_OSCILLATION** (`0x88`): Oszillation an/aus
- **TOGGLE_ROTATION** (`0x89`): Rotation an/aus
- **TOGGLE_PUSLATING** (`0x8D`): Pulsation an/aus
- **TOGGLE_HW** (`0x8A`): Warmwasser an/aus
- **TOGGLE_BUZZER** (`0x65`): Signalton an/aus

### 4.7 Düsen-Wartung (NOZZLE_MAINTANNCE)
**Befehlscode:** `0x82` (130)
**Parameter:** 2 Bytes
- Byte 0: `0x01` (Wartungsmodus aktivieren)
- Byte 1: `0x00` (Reserved)

---

## 5. Befehlsbeispiele

### Beispiel 1: Programm starten
```
Input-Parameter:
  Gender: 0 (Lady)
  Pulsate: 1
  Rotate: 0
  Oscillate: 1
  Pumpstate: 3
  Nozzlestate: 3
  Temperature: 3

Schritt 1: Bitmask berechnen
  bitmask = ((0 + 1) & 3) | ((1 & 1) << 2) | ((0 & 1) << 3) | ((1 & 1) << 4)
  bitmask = 0x15 (00010101)

Schritt 2: Befehlsblock erstellen
  Adresse: FF FF
  Command: 05 (Write Coil)
  Subcommand: 8E (START_PROGRAM)
  Data: 15 03 03 03

  Roh-Bytes: FF FF 05 8E 15 03 03 03

Schritt 3: In ASCII konvertieren
  "FFFF058E15030303"

Schritt 4: LRC berechnen (XOR aller ASCII-Bytes)
  LRC = 'F' ^ 'F' ^ 'F' ^ 'F' ^ '0' ^ '5' ^ '8' ^ 'E' ^ ...
  LRC = 0x?? → zu ASCII "XX"

Schritt 5: Kompletter Befehl
  ":FFFF058E15030303XX\r\n"
```

### Beispiel 2: Temperatur auf Stufe 4 setzen
```
Schritt 1: Parameter
  Command: 05 (Write Coil)
  Subcommand: 85 (SET_TEMP)
  Data: 04

Schritt 2: Roh-Bytes
  FF FF 05 85 04

Schritt 3: ASCII-Konvertierung
  "FFFF058504"

Schritt 4: LRC berechnen
  LRC → ASCII

Schritt 5: Kompletter Befehl
  ":FFFF058504XX\r\n"
```

### Beispiel 3: Oszillation umschalten
```
Command: 05 (Write Coil)
Subcommand: 88 (TOGGLE_OSCILLATION)
Data: (keine)

Roh-Bytes: FF FF 05 88
ASCII: "FFFF0588"
Befehl: ":FFFF0588XX\r\n"
```

---

## 6. BLE-Verbindungsablauf

### 1. Scannen nach Geräten
```java
// Scan-Filter für NUS-Service
ScanFilter filter = new ScanFilter.Builder()
    .setServiceUuid(ParcelUuid.fromString("6E400001-B5A3-F393-E0A9-E50E24DCCA9E"))
    .build();

// Scan starten
bluetoothLeScanner.startScan(filters, settings, scanCallback);
```

### 2. Verbindung herstellen
```java
device.connectGatt(context, false, gattCallback);
```

### 3. Services entdecken
```java
gatt.discoverServices();
```

### 4. Benachrichtigungen aktivieren
```java
// Notifications für TX Characteristic aktivieren
gatt.setCharacteristicNotification(characteristicTx, true);

// CCCD-Descriptor setzen
BluetoothGattDescriptor descriptor = characteristicTx.getDescriptor(CCCD_UUID);
descriptor.setValue(BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE);
gatt.writeDescriptor(descriptor);
```

### 5. Befehle senden
```java
// Befehl in RX Characteristic schreiben
characteristicRx.setValue(commandBytes);
gatt.writeCharacteristic(characteristicRx);
```

### 6. BLE-Paketgröße beachten
BLE unterstützt maximal 20 Bytes pro Paket. Die App teilt größere Befehle automatisch auf:
```java
if (value.length > 20) {
    // In 20-Byte-Blöcke aufteilen
    int byteBlocks = (value.length / 20) + 1;
    for (int i = 0; i < byteBlocks; i++) {
        byte[] byteBlock = new byte[20];
        // Block füllen und zur Queue hinzufügen
    }
}
```

**Write Period:** 100ms zwischen Schreibvorgängen

---

## 7. ESP32 Implementierungshinweise

### Benötigte Bibliotheken
```cpp
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
```

### Service und Characteristics definieren
```cpp
#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
```

### LRC-Berechnung (C++)
```cpp
uint8_t calculateLRC(uint8_t* data, size_t length) {
    uint8_t lrc = 0;
    for (size_t i = 0; i < length; i++) {
        lrc ^= data[i];
    }
    return lrc;
}
```

### Byte zu ASCII-Hex
```cpp
void byteToAsciiHex(uint8_t byte, char* output) {
    sprintf(output, "%02X", byte);
}
```

### Befehl erstellen
```cpp
String createCommand(uint8_t cmd, uint8_t subcmd, uint8_t* data, size_t dataLen) {
    String command = ":FFFF";

    // Command byte
    char hex[3];
    sprintf(hex, "%02X", cmd);
    command += hex;

    // Subcommand
    sprintf(hex, "%02X", subcmd);
    command += hex;

    // Data
    for (size_t i = 0; i < dataLen; i++) {
        sprintf(hex, "%02X", data[i]);
        command += hex;
    }

    // LRC berechnen (auf ASCII-Bytes!)
    uint8_t lrc = 0;
    for (size_t i = 1; i < command.length(); i++) {
        lrc ^= command[i];
    }
    sprintf(hex, "%02X", lrc);
    command += hex;

    // CRLF
    command += "\r\n";

    return command;
}
```

### Beispiel: Temperatur setzen
```cpp
void setTemperature(uint8_t temp) {
    uint8_t data[] = {temp};
    String cmd = createCommand(0x05, 0x85, data, 1);

    pCharacteristicRX->setValue(cmd.c_str());
    pCharacteristicRX->notify();
}
```

---

## 8. Wichtige Erkenntnisse

1. **Nordic UART Service**: Standard BLE-Service, gut dokumentiert
2. **Modbus-ähnlich**: Verwendet Modbus-Konzepte (Coils, Register)
3. **ASCII-Codierung**: Alle Daten werden als ASCII-Hex übertragen
4. **20-Byte-Limit**: BLE-Paketgröße beachten
5. **LRC-Checksum**: XOR-basierte Fehlerprüfung
6. **Write Period**: 100ms Wartezeit zwischen Befehlen

---

## 9. Nächste Schritte für ESP32-Implementierung

1. **BLE-Server einrichten** mit NUS-UUIDs
2. **Callback für RX Characteristic** implementieren
3. **Befehlsparser** schreiben (ASCII → Binär, LRC-Prüfung)
4. **Befehls-Handler** für jeden Befehlstyp
5. **Statusmeldungen** über TX Characteristic senden
6. **Testing** mit der ViClean App

---

## 10. Referenzen

- **Nordic UART Service**: https://developer.nordicsemi.com/nRDK/nRDK_v1.1.0/doc/1.1.0/s110/html/a00066.html
- **Modbus Protocol**: https://www.modbus.org/specs.php
- **ESP32 BLE**: https://github.com/espressif/arduino-esp32/tree/master/libraries/BLE

---

**Erstellt:** 2026-02-12
**Version:** 1.0
**Quelle:** Reverse Engineering von ViClean_1.0_APKPure.apk
