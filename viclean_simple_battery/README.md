# ViClean Simple Battery - Batterieschonende Version

Basiert auf `viclean_simple`, erweitert um Deep Sleep fuer den Akkubetrieb.

## Hardware

- **LilyGO T-Display v1.1** (ESP32 mit 135x240 TFT)
- **450mAh LiPo-Akku** (ueber JST-Anschluss des T-Display)
- **2 Buttons:**
  - GPIO 35 (links) - SELECT (weckt aus Sleep)
  - GPIO 0 (rechts) - START/STOP (weckt aus Sleep)

## Stromsparmodus

Das Geraet geht nach **60 Sekunden Inaktivitaet** automatisch in Deep Sleep.

| Zustand | Verbrauch | Akkulaufzeit (450mAh) |
|---------|-----------|----------------------|
| Aktiv (BLE + Display) | ~130-180mA | ~3 Stunden |
| Deep Sleep | ~10-50uA | Monate |

### Ablauf

1. **Normalbetrieb:** Display an, BLE verbunden, Sleep-Countdown oben sichtbar (`Zzz 45s`)
2. **10 Sek. vor Sleep:** Display dimmt stark als Warnung
3. **Sleep:** Display aus, BLE aus, ESP32 im Deep Sleep
4. **Aufwachen:** Beliebigen Button druecken - ESP32 startet neu, verbindet sich automatisch

Waehrend ein Programm laeuft, wird **kein Sleep** ausgeloest.

## Display Layout

```
+----------------------------------+
| ViClean  Zzz 45s        CONN    |  <- Sleep-Timer + Status
|                                  |
|     +---+                        |
|     | W |      M                 |  <- Programmwahl
|     +---+                        |
|                                  |
|          STOPPED                 |  <- Programm Status
|                                  |
| SELECT               START      |  <- Button Labels
+----------------------------------+
```

## Bedienung

1. **Aufwachen:** Beliebigen Button druecken
2. **Programm waehlen:** Linke Taste (GPIO 35) - wechselt W/M
3. **Starten:** Rechte Taste (GPIO 0) - startet Programm
4. **Stoppen:** Rechte Taste erneut
5. **Sleep:** Automatisch nach 60s ohne Eingabe

## Konfiguration

Konstanten am Anfang der `.ino` Datei:

```cpp
#define SLEEP_TIMEOUT_SEC    60    // Sekunden bis Deep Sleep
#define DIM_WARNING_SEC      10    // Sekunden vor Sleep: Display dimmen
```

## Installation

1. Arduino IDE oeffnen
2. Board: LilyGo T-Display
3. Projekt oeffnen: `viclean_simple_battery.ino`
4. Hochladen
5. Fertig!

## Unterschied zu viclean_simple

| Feature | Simple | Simple Battery |
|---------|--------|----------------|
| Deep Sleep | Nein | Ja (60s Timeout) |
| Akku-Betrieb | Stunden | Wochen/Monate |
| Sleep-Timer | Nein | Ja (Countdown) |
| Display-Dimming | Nein | Ja (Warnung vor Sleep) |
| Wake-Up | - | Beide Buttons |
| Codegroesse | ~420 Zeilen | ~500 Zeilen |

## Troubleshooting

**Geraet wacht nicht auf:**
- Beide Buttons muessen korrekt angeschlossen sein
- GPIO 0 und GPIO 35 sind die Wake-Up-Quellen

**Display bleibt nach Aufwachen dunkel:**
- Kurz warten, ESP32 macht vollstaendigen Neustart
- Serial Monitor (115200 baud) zeigt Wake-Reason

**Akku haelt trotzdem nur kurz:**
- `SLEEP_TIMEOUT_SEC` reduzieren (z.B. auf 30)
- Pruefen ob Geraet wirklich in Sleep geht (Display muss komplett aus sein)

## Code-Struktur

```
viclean_simple_battery/
├── viclean_simple_battery.ino   # Hauptcode mit Deep Sleep
├── User_Setup.h                 # TFT Display Config
└── README.md                    # Diese Datei
```
