# ViClean Simple - Minimale UI mit 2 Programmen

Einfache Steuerung mit nur 2 Tasten und großen Buchstaben auf dem Display.

## Hardware

- **LilyGO T-Display v1.1** (ESP32 mit 135x240 TFT)
- **2 Buttons:**
  - GPIO 35 (links) - SELECT
  - GPIO 0 (rechts) - START/STOP

## Display Layout

```
┌─────────────────────────────────┐
│ ViClean              CONN       │  <- Status oben
│                                 │
│     ┌───┐                       │
│     │ W │      M                │  <- W = grün (ausgewählt)
│     └───┘                       │     M = grau
│                                 │
│         RUNNING                 │  <- Programm Status
│                                 │
│ SELECT              START       │  <- Button Labels
└─────────────────────────────────┘
```

## Programme

### W (Woman/Lady)
- Gender: Lady
- HarmonicWave: ON (Oscillation + Rotation)
- Pulsation: OFF
- Alle Stufen: 3 (Pump, Nozzle, Temp)

### M (Men/Gent)
- Gender: Gent
- HarmonicWave: ON (Oscillation + Rotation)
- Pulsation: OFF
- Alle Stufen: 3 (Pump, Nozzle, Temp)

## Bedienung

1. **Programm wählen:**
   - Drücke **linke Taste** (GPIO 35)
   - Wechselt zwischen W und M
   - Ausgewähltes Programm hat grünen Rahmen
   - Nur möglich wenn STOPPED

2. **Programm starten:**
   - Drücke **rechte Taste** (GPIO 0)
   - Display zeigt "RUNNING"
   - BLE trennt kurz und verbindet neu (normal!)
   - Gerät startet physisch

3. **Programm stoppen:**
   - Drücke **rechte Taste** (GPIO 0)
   - Display zeigt "STOPPED"

## Installation

1. **Arduino IDE öffnen**
2. **Board wählen:** LilyGo T-Display
3. **Projekt öffnen:** `viclean_simple.ino`
4. **Hochladen**
5. **Fertig!**

## Technische Details

### BLE-Verhalten
- Automatische Verbindung zu ViClean
- Auto-Reconnect bei Trennung
- START trennt BLE (normales Verhalten)
- Packet-Splitting für lange Befehle (>20 Bytes)

### Button-Debouncing
- 300ms Debounce-Zeit
- Interrupt-basiert für schnelle Reaktion
- GPIO 35 ist input-only (keine Pull-up möglich)

### Display-Update
- Automatisches Update bei:
  - Programmwahl
  - START/STOP
  - BLE Connect/Disconnect

## Unterschied zu viclean_serial_bridge_v2

| Feature | Simple | Bridge V2 |
|---------|--------|-----------|
| UI | Große Buchstaben | Serial Monitor |
| Bedienung | 2 Tasten | Tastatur-Befehle |
| Programme | 2 fest | Viele + Custom |
| Einstellungen | Keine | Alle (temp, pump, etc.) |
| Test-Befehle | Nein | Ja (t1-t10, raw, etc.) |
| Größe | ~300 Zeilen | ~800 Zeilen |

## Troubleshooting

**Display bleibt schwarz:**
- Überprüfe Backlight (TFT_BL Pin 4)
- User_Setup.h muss im selben Ordner sein

**Buttons reagieren nicht:**
- GPIO 35 darf kein PULLUP haben (input-only)
- GPIO 0 hat internen PULLUP

**Keine BLE-Verbindung:**
- ViClean muss in BLE-Reichweite sein
- Prüfe Serial Monitor (115200 baud)
- "ViClean gefunden" sollte erscheinen

**Programm startet nicht:**
- Warte bis "CONN" oben rechts erscheint
- BLE-Verbindung muss stehen
- Nach START trennt BLE kurz (normal!)

## Code-Struktur

```
viclean_simple/
├── viclean_simple.ino   # Hauptcode
├── User_Setup.h         # TFT Display Config
└── README.md            # Diese Datei
```

## Lizenz

Dieses Projekt wurde durch Reverse Engineering der ViClean App erstellt.
Nur für private Nutzung.
