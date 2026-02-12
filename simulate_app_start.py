#!/usr/bin/env python3
"""
Simuliert exakt was die ViClean App beim START sendet
"""

def calculate_lrc(data_bytes):
    lrc = 0
    for byte in data_bytes:
        lrc ^= byte
    return lrc

def create_command(modbus_cmd, subcmd, data):
    """Erstellt Befehl wie in der App"""
    command = ":FFFF"

    # Modbus Command
    command += f"{modbus_cmd:02X}"

    # Subcommand
    command += f"{subcmd:02X}"

    # Data
    for d in data:
        command += f"{d:02X}"

    # Berechne LRC über alles nach dem :
    command_for_lrc = command[1:]  # Ohne :
    lrc = calculate_lrc(command_for_lrc.encode('ascii'))
    command += f"{lrc:02X}"
    command += "\r\n"

    return command

# Simuliere was die App beim START sendet
print("=== SIMULATION: ViClean App START ===\n")

print("Scenario: Lady, HarmonicWave ON, Puls OFF, Pump=3, Nozzle=3, Temp=3")
print()

# 1. Parameter wie in MainActivity.java Zeile 531-537
gender = 0  # Lady
oscillate = 1  # HarmonicWave enabled
rotate = 1  # HarmonicWave enabled
pulsate = 0
pumpstate = 3
nozzlestate = 3
temperature = 3

# 2. Bitmask wie in VicBluetoothManager.java Zeile 501-502
bitmaskParam = ((gender + 1) & 0x03) | 0
print(f"Schritt 1: bitmaskParam = ((gender+1) & 0x03) | 0")
print(f"         = (({gender}+1) & 0x03) | 0")
print(f"         = ({gender+1} & 0x03)")
print(f"         = 0x{bitmaskParam:02X} = {bitmaskParam}")
print()

bitmaskParam2 = (((pulsate & 0x01) << 4) |
                ((rotate & 0x01) << 3) |
                ((oscillate & 0x01) << 2) |
                bitmaskParam)

print(f"Schritt 2: bitmaskParam2 = (pulsate<<4) | (rotate<<3) | (oscillate<<2) | bitmaskParam")
print(f"         = ({pulsate}<<4) | ({rotate}<<3) | ({oscillate}<<2) | {bitmaskParam}")
print(f"         = {pulsate<<4} | {rotate<<3} | {oscillate<<2} | {bitmaskParam}")
print(f"         = 0x{bitmaskParam2:02X} = {bitmaskParam2}")
print()

# 3. Befehl erstellen
print("Schritt 3: Erstelle Befehl")
print(f"  Command: 0x05 (Write Coil)")
print(f"  Subcmd:  0x8E (START_PROGRAM)")
print(f"  Param1:  0x{bitmaskParam2:02X} (bitmask)")
print(f"  Param2:  0x{pumpstate:02X} (pump)")
print(f"  Param3:  0x{nozzlestate:02X} (nozzle)")
print(f"  Param4:  0x{temperature:02X} (temp)")
print()

data = [bitmaskParam2, pumpstate, nozzlestate, temperature]
cmd = create_command(0x05, 0x8E, data)

print(f"FINALER BEFEHL: {cmd.strip()}")
print()

# Vergleiche mit dem was wir senden
our_cmd = ":FFFF058E0D0303030F\r\n"
print(f"Unser Befehl:   {our_cmd.strip()}")
print()

if cmd == our_cmd:
    print("✓ BEFEHLE SIND IDENTISCH!")
else:
    print("✗ BEFEHLE UNTERSCHIEDLICH!")
    print(f"  App würde senden: {cmd.strip()}")
    print(f"  Wir senden:       {our_cmd.strip()}")

print()
print("=== ANDERE SZENARIEN ===\n")

# Test minimal
print("Scenario: Lady, alles OFF, 1-1-1")
gender = 0
oscillate = 0
rotate = 0
pulsate = 0
pumpstate = 1
nozzlestate = 1
temperature = 1

bitmaskParam = ((gender + 1) & 0x03) | 0
bitmaskParam2 = (((pulsate & 0x01) << 4) |
                ((rotate & 0x01) << 3) |
                ((oscillate & 0x01) << 2) |
                bitmaskParam)
data = [bitmaskParam2, pumpstate, nozzlestate, temperature]
cmd = create_command(0x05, 0x8E, data)
print(f"Befehl: {cmd.strip()}")
print(f"Bitmask: 0x{bitmaskParam2:02X}")
print()

# Test Gent
print("Scenario: Gent, HarmonicWave ON, 3-3-3")
gender = 1
oscillate = 1
rotate = 1
pulsate = 0
pumpstate = 3
nozzlestate = 3
temperature = 3

bitmaskParam = ((gender + 1) & 0x03) | 0
bitmaskParam2 = (((pulsate & 0x01) << 4) |
                ((rotate & 0x01) << 3) |
                ((oscillate & 0x01) << 2) |
                bitmaskParam)
data = [bitmaskParam2, pumpstate, nozzlestate, temperature]
cmd = create_command(0x05, 0x8E, data)
print(f"Befehl: {cmd.strip()}")
print(f"Bitmask: 0x{bitmaskParam2:02X}")
