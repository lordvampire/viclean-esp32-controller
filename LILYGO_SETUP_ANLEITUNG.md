# LilyGO T-Display v1.1 - Setup-Anleitung

## Hardware-Übersicht

**LilyGO TTGO T-Display v1.1**
- ESP32 Mikrocontroller
- 135x240 TFT Display (ST7789)
- 2x Druckknöpfe (GPIO35, GPIO0)
- Integrierter USB-C Port
- Integrierte Antenne für BLE/WiFi

## Erforderliche Software

### 1. Arduino IDE
Download: https://www.arduino.cc/en/software

### 2. ESP32 Board-Support
1. Arduino IDE öffnen
2. **Datei → Voreinstellungen**
3. Bei "Zusätzliche Boardverwalter-URLs" hinzufügen:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
4. **Werkzeuge → Board → Boardverwalter**
5. "ESP32" suchen und installieren

### 3. TFT_eSPI Bibliothek
1. **Sketch → Bibliothek einbinden → Bibliotheken verwalten**
2. "TFT_eSPI" von Bodmer suchen und installieren
3. **WICHTIG:** Konfiguration anpassen (siehe unten)

## TFT_eSPI Konfiguration

### Methode 1: User_Setup_Select.h bearbeiten (empfohlen)

1. Bibliotheksordner finden:
   - **Windows**: `C:\Users\[USER]\Documents\Arduino\libraries\TFT_eSPI\`
   - **Linux**: `~/Arduino/libraries/TFT_eSPI/`
   - **Mac**: `~/Documents/Arduino/libraries/TFT_eSPI/`

2. Datei `User_Setup_Select.h` öffnen

3. Alle Zeilen auskommentieren (mit `//`) und folgende Zeile aktivieren:
   ```cpp
   #include <User_Setups/Setup25_TTGO_T_Display.h>
   ```

### Methode 2: User_Setup.h direkt bearbeiten

Falls Setup25 nicht existiert, `User_Setup.h` mit folgenden Einstellungen konfigurieren:

```cpp
#define ST7789_DRIVER

#define TFT_WIDTH  135
#define TFT_HEIGHT 240

#define CGRAM_OFFSET

#define TFT_MOSI 19
#define TFT_SCLK 18
#define TFT_CS   5
#define TFT_DC   16
#define TFT_RST  23
#define TFT_BL   4

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF

#define SMOOTH_FONT

#define SPI_FREQUENCY  27000000
#define SPI_READ_FREQUENCY  20000000
#define SPI_TOUCH_FREQUENCY  2500000
```

## Board-Einstellungen in Arduino IDE

Nach dem Öffnen von `lilygo_tdisplay_viclean.ino`:

```
Werkzeuge → Board → ESP32 Arduino → ESP32 Dev Module

Weitere Einstellungen:
- Upload Speed: 921600
- CPU Frequency: 240MHz (WiFi/BT)
- Flash Frequency: 80MHz
- Flash Mode: QIO
- Flash Size: 4MB (32Mb)
- Partition Scheme: Default 4MB with spiffs
- Core Debug Level: None
- PSRAM: Disabled
- Port: [Dein USB-Port auswählen]
```

## Code hochladen

1. **lilygo_tdisplay_viclean.ino** in Arduino IDE öffnen
2. Board und Port auswählen (siehe oben)
3. **Sketch → Hochladen** (oder Strg+U)
4. Warten bis "Hard resetting via RTS pin..." erscheint
5. Board sollte neu starten

## Verwendung

### Display-Anzeige

**Header (Blau):**
- "ViClean BLE" - Titel

**Status-Zeile:**
- Zeigt aktuellen Zustand (Suchen, Verbunden, etc.)

**Log-Bereich:**
- Zeigt gesendete Befehle und Antworten
- Scrollt automatisch

**Button-Beschriftungen (unten):**
- Links: "TEST" - Testbefehle durchlaufen
- Rechts: "START"/"STOP" - Programm starten/stoppen

### Button-Funktionen

#### Linker Button (GPIO35) - TEST
Sendet nacheinander Test-Befehle:
1. Buzzer umschalten
2. Temperatur auf Stufe 2
3. Temperatur auf Stufe 4
4. Wasserpumpe auf Stufe 2
5. Wasserpumpe auf Stufe 4
6. Düsenposition auf Stufe 2
7. Düsenposition auf Stufe 4
8. Oszillation umschalten
9. Rotation umschalten
10. Pulsation umschalten

#### Rechter Button (GPIO0) - START/STOP
- **START**: Startet Standard-Programm
  - Gender: Lady (0)
  - Oszillation: An
  - Rotation: Aus
  - Pulsation: Aus
  - Alle Werte auf Stufe 3
- **STOP**: Beendet aktuelles Programm

### Farbcodes

- **Blau**: Header
- **Cyan**: Info-Meldungen
- **Grün**: Erfolg (Verbindung, Befehl gesendet)
- **Gelb**: Warnungen (empfangene Daten)
- **Rot**: Fehler
- **Weiß**: Normale Logs

## Verbindungsablauf

1. **Power On**: Display startet, zeigt "Initialisiere..."
2. **Scanning**: "Suche ViClean..." - ESP32 scannt nach BLE-Geräten
3. **Found**: "ViClean gefunden!" - Gerät mit NUS-Service entdeckt
4. **Connecting**: "Verbinde..." - Baut Verbindung auf
5. **Connected**: "Verbunden!" - Zeigt Gerätename und MAC-Adresse
6. **Ready**: "Bereit für Befehle" - Buttons sind aktiv

## Troubleshooting

### Display bleibt schwarz
- **Lösung**: TFT_eSPI Konfiguration prüfen
- Setup25_TTGO_T_Display.h muss aktiviert sein
- Bibliothek neu installieren

### "Compilation error: TFT_eSPI.h: No such file"
- **Lösung**: TFT_eSPI Bibliothek installieren
- Sketch → Bibliothek einbinden → Bibliotheken verwalten → "TFT_eSPI"

### Display zeigt falsches Bild/Farben
- **Lösung**: TFT_WIDTH und TFT_HEIGHT in User_Setup.h prüfen
- Sollte 135x240 sein
- CGRAM_OFFSET muss definiert sein

### "A fatal error occurred: Failed to connect"
- **Lösung**:
  - USB-Kabel prüfen (muss Daten übertragen können)
  - Richtigen COM-Port wählen
  - Board-Treiber installieren (CP210x oder CH340)

### Keine BLE-Verbindung
- **Lösung**:
  - ViClean Gerät einschalten
  - Andere BLE-Verbindungen trennen
  - ESP32 neu starten (RST-Button)
  - Seriellen Monitor öffnen (115200 Baud) für Debug-Infos

### Buttons reagieren nicht
- **Lösung**:
  - Warten bis "Verbunden!" angezeigt wird
  - Nur ein Befehl pro Sekunde senden
  - Button fest drücken

### Display-Rotation falsch
- **Lösung**: In Code ändern:
  ```cpp
  tft.setRotation(1); // 0, 1, 2, oder 3
  ```
  - 0 = Portrait
  - 1 = Landscape (Standard)
  - 2 = Portrait invertiert
  - 3 = Landscape invertiert

## Erweiterungsmöglichkeiten

### 1. WiFi Web-Interface hinzufügen
```cpp
#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "DeinWiFi";
const char* password = "DeinPasswort";

WebServer server(80);
```

### 2. Mehr Buttons/Befehle
GPIO-Pins verfügbar: 12, 13, 25, 26, 27, 32, 33

### 3. Batterie-Anzeige
```cpp
int batteryLevel = analogRead(34); // ADC Pin
```

### 4. MQTT Integration
```cpp
#include <PubSubClient.h>
```

### 5. Gespeicherte Profile
```cpp
#include <Preferences.h>
Preferences preferences;
```

## Serielle Konsole (Debug)

Öffne den Seriellen Monitor (115200 Baud):
```
Werkzeuge → Serieller Monitor
```

Ausgabe zeigt:
- BLE-Scan-Ergebnisse
- Verbindungsstatus
- Gesendete Befehle
- Empfangene Daten

## Pinout-Referenz

| Pin | Funktion | Verwendung |
|-----|----------|------------|
| GPIO35 | Button 1 | Linker Button (TEST) |
| GPIO0 | Button 2 | Rechter Button (START/STOP) |
| GPIO5 | TFT_CS | Display Chip Select |
| GPIO16 | TFT_DC | Display Data/Command |
| GPIO23 | TFT_RST | Display Reset |
| GPIO18 | TFT_SCLK | Display SPI Clock |
| GPIO19 | TFT_MOSI | Display SPI Data |
| GPIO4 | TFT_BL | Display Backlight |
| GPIO34 | ADC | Batterie (optional) |

**Freie GPIOs für Erweiterungen:** 12, 13, 25, 26, 27, 32, 33

## Weitere Ressourcen

- **LilyGO GitHub**: https://github.com/Xinyuan-LilyGO/TTGO-T-Display
- **TFT_eSPI GitHub**: https://github.com/Bodmer/TFT_eSPI
- **ESP32 BLE Docs**: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/bluetooth/index.html

---

**Viel Erfolg mit deinem ViClean Controller!**
