#!/bin/bash
# TFT_eSPI Bibliothek automatisch installieren

echo "=== TFT_eSPI Bibliothek Installation ==="

# Arduino Libraries Verzeichnis
ARDUINO_LIB_DIR="$HOME/Arduino/libraries"

# Erstelle Libraries-Ordner falls nicht vorhanden
mkdir -p "$ARDUINO_LIB_DIR"

# Wechsel in Libraries-Ordner
cd "$ARDUINO_LIB_DIR"

# Prüfe ob TFT_eSPI bereits existiert
if [ -d "TFT_eSPI" ]; then
    echo "TFT_eSPI existiert bereits. Lösche alte Version..."
    rm -rf TFT_eSPI
fi

# Clone TFT_eSPI von GitHub
echo "Lade TFT_eSPI herunter..."
git clone https://github.com/Bodmer/TFT_eSPI.git

if [ $? -eq 0 ]; then
    echo ""
    echo "✓ TFT_eSPI erfolgreich installiert!"
    echo ""
    echo "Nächste Schritte:"
    echo "1. Arduino IDE neu starten"
    echo "2. Code kompilieren"
    echo ""
    echo "WICHTIG: TFT_eSPI Konfiguration anpassen:"
    echo "Datei: $ARDUINO_LIB_DIR/TFT_eSPI/User_Setup_Select.h"
    echo "Zeile aktivieren: #include <User_Setups/Setup25_TTGO_T_Display.h>"
else
    echo ""
    echo "✗ Fehler beim Download. Bitte manuell installieren:"
    echo "1. https://github.com/Bodmer/TFT_eSPI/archive/master.zip"
    echo "2. Entpacken nach: $ARDUINO_LIB_DIR/TFT_eSPI"
fi
