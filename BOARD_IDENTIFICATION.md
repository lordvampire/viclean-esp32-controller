# LilyGO T-Display - Board-Identifikation

## Methode 1: Auf dem Board-Aufdruck schauen

Schau auf die Rückseite oder Vorderseite deines Boards:

- **"TTGO T-Display"** oder **"T-Display v1.1"** → Original ESP32
- **"T-Display S3"** oder **"LILYGO T-Display S3"** → ESP32-S3

## Methode 2: USB-Port prüfen

- **Micro-USB** → Fast sicher Original T-Display (ESP32)
- **USB-C mit "USB" Aufdruck** → Könnte beides sein
- **USB-C mit "OTG" erwähnt** → T-Display S3 (ESP32-S3)

## Methode 3: Display-Größe

- **135 x 240 px** → Original T-Display (ESP32)
- **170 x 320 px** → T-Display S3 (ESP32-S3)

Dein **v1.1** ist höchstwahrscheinlich das **Original T-Display mit ESP32**.

## Methode 4: Chip-ID auslesen (sicherste Methode)

1. Verbinde das Board per USB
2. Öffne Arduino IDE
3. Öffne: **Datei → Beispiele → ESP32 → ChipID → GetChipID**
4. Wähle **ESP32 Dev Module** als Board
5. Hochladen und Seriellen Monitor öffnen (115200 Baud)

**Ausgabe interpretieren:**
- `Chip Model: ESP32-D0WDQ6` → **Original T-Display** (ESP32)
- `Chip Model: ESP32-S3` → **T-Display S3** (ESP32-S3)

---

# Board-Einstellungen für Arduino IDE

## Für T-Display v1.1 (Original ESP32) - WAHRSCHEINLICH DEINS

```
Werkzeuge → Board → ESP32 Arduino → ESP32 Dev Module

Upload Speed: 921600
CPU Frequency: 240MHz (WiFi/BT)
Flash Frequency: 80MHz
Flash Mode: QIO
Flash Size: 4MB (32Mb)
Partition Scheme: Default 4MB with spiffs
Core Debug Level: None
PSRAM: Disabled
```

**Code verwenden:** `lilygo_tdisplay_viclean.ino` (funktioniert perfekt)

---

## Für T-Display S3 (ESP32-S3) - Falls du doch S3 hast

```
Werkzeuge → Board → ESP32 Arduino → ESP32S3 Dev Module

Upload Speed: 921600
USB Mode: Hardware CDC and JTAG
USB CDC On Boot: Enabled
CPU Frequency: 240MHz (WiFi)
Flash Mode: QIO 80MHz
Flash Size: 16MB (128Mb)
Partition Scheme: 16M Flash (3MB APP/9.9MB FATFS)
PSRAM: OPI PSRAM
```

**Code-Anpassung nötig:** Buttons sind auf anderen Pins!

---

# Wenn du ein T-Display S3 hast (Code-Änderungen)

Die Button-Pins sind unterschiedlich:

**T-Display (ESP32) - Original:**
```cpp
#define BUTTON_LEFT  35  // Linker Button
#define BUTTON_RIGHT 0   // Rechter Button
```

**T-Display S3 (ESP32-S3):**
```cpp
#define BUTTON_LEFT  14  // Linker Button
#define BUTTON_RIGHT 0   // Rechter Button (bleibt gleich)
```

Wenn du S3 hast, ändere im Code:
```cpp
// Zeile 21 ändern von:
#define BUTTON_LEFT  35

// zu:
#define BUTTON_LEFT  14
```

---

# Schnelltest - Welches Board habe ich?

Upload diesen Mini-Test-Code:

```cpp
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n=== Board Information ===");
  Serial.printf("Chip Model: %s\n", ESP.getChipModel());
  Serial.printf("Chip Revision: %d\n", ESP.getChipRevision());
  Serial.printf("Flash Size: %d MB\n", ESP.getFlashChipSize() / (1024 * 1024));
  Serial.printf("PSRAM Size: %d MB\n", ESP.getPsramSize() / (1024 * 1024));

  if (strcmp(ESP.getChipModel(), "ESP32-S3") == 0) {
    Serial.println("\n>>> Du hast ein T-Display S3!");
    Serial.println(">>> Wähle Board: ESP32S3 Dev Module");
  } else {
    Serial.println("\n>>> Du hast ein T-Display (Original)!");
    Serial.println(">>> Wähle Board: ESP32 Dev Module");
  }
}

void loop() {
  delay(1000);
}
```

1. Code in neue Sketch kopieren
2. Board: **ESP32 Dev Module** wählen
3. Upload
4. Seriellen Monitor öffnen (115200 Baud)
5. Ausgabe lesen

---

# Zusammenfassung

**Dein "T-Display v1.1" ist höchstwahrscheinlich:**
- ✅ **Original T-Display mit ESP32**
- ✅ Board-Auswahl: **ESP32 Dev Module**
- ✅ Code funktioniert ohne Änderungen

**Falls es doch S3 ist:**
- 🔧 Board-Auswahl: **ESP32S3 Dev Module**
- 🔧 BUTTON_LEFT von 35 auf 14 ändern
- 🔧 USB CDC On Boot: Enabled
