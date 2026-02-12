#!/bin/bash
# TFT_eSPI für LilyGO T-Display konfigurieren

SETUP_FILE="$HOME/Arduino/libraries/TFT_eSPI/User_Setup_Select.h"

echo "=== TFT_eSPI Konfiguration für T-Display ==="

if [ ! -f "$SETUP_FILE" ]; then
    echo "✗ Fehler: TFT_eSPI nicht gefunden!"
    echo "Installiere zuerst die Bibliothek."
    exit 1
fi

# Backup erstellen
cp "$SETUP_FILE" "$SETUP_FILE.backup"
echo "✓ Backup erstellt: $SETUP_FILE.backup"

# Alle includes auskommentieren
sed -i 's/^#include <User_Setup.h>/\/\/#include <User_Setup.h>/' "$SETUP_FILE"

# Setup25 aktivieren wenn auskommentiert
sed -i 's/^\/\/ *#include <User_Setups\/Setup25_TTGO_T_Display.h>/#include <User_Setups\/Setup25_TTGO_T_Display.h>/' "$SETUP_FILE"

# Prüfen ob Setup25 schon aktiv ist, sonst hinzufügen
if ! grep -q "^#include <User_Setups/Setup25_TTGO_T_Display.h>" "$SETUP_FILE"; then
    echo "" >> "$SETUP_FILE"
    echo "// LilyGO T-Display Configuration" >> "$SETUP_FILE"
    echo "#include <User_Setups/Setup25_TTGO_T_Display.h>" >> "$SETUP_FILE"
fi

echo "✓ TFT_eSPI konfiguriert für T-Display"
echo ""
echo "Nächste Schritte:"
echo "1. Arduino IDE neu starten"
echo "2. Code kompilieren"
echo ""
echo "Falls Probleme auftreten, Backup wiederherstellen:"
echo "cp $SETUP_FILE.backup $SETUP_FILE"
