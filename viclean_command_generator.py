#!/usr/bin/env python3
"""
ViClean DuschWC Command Generator
Proof-of-Concept für die Generierung von Bluetooth-Befehlen

Verwendung:
    python3 viclean_command_generator.py
"""

class VicCommand:
    """ViClean Befehlscodes"""
    START_CW_NOZZLE = 0x70
    START_CCW_NOZZLE = 0x71
    START_CW_WIGGLE = 0x72
    START_PROGRAM = 0x8E
    BREAK_PROGRAM = 0x8F
    SET_NOZZLE_POSITION = 0x81
    SET_TEMP = 0x85
    SET_WATER_PUMP = 0x7E
    TOGGLE_OSCILLATION = 0x88
    TOGGLE_ROTATION = 0x89
    TOGGLE_PULSATING = 0x8D
    TOGGLE_HW = 0x8A
    TOGGLE_BUZZER = 0x65
    NOZZLE_MAINTENANCE = 0x82


class ModbusCommand:
    """Modbus Befehlscodes"""
    READ_REGISTER = 0x03
    WRITE_COIL = 0x05
    WRITE_REGISTER = 0x06


def calculate_lrc(data_bytes):
    """
    Berechnet die LRC (Longitudinal Redundancy Check) Checksum

    Args:
        data_bytes: Bytes für die Checksummenberechnung

    Returns:
        LRC als Integer
    """
    lrc = 0
    for byte in data_bytes:
        lrc ^= byte
    return lrc


def bytes_to_ascii_hex(data_bytes):
    """
    Konvertiert Bytes in ASCII-Hex-String

    Args:
        data_bytes: Liste von Bytes

    Returns:
        ASCII-Hex-String
    """
    return ''.join(f'{b:02X}' for b in data_bytes)


def create_command(modbus_cmd, subcmd, data=None):
    """
    Erstellt einen vollständigen ViClean-Befehl

    Args:
        modbus_cmd: Modbus-Befehlscode (0x03, 0x05, 0x06)
        subcmd: Subbefehl (VicCommand)
        data: Liste von Daten-Bytes (optional)

    Returns:
        Vollständiger Befehl als String
    """
    if data is None:
        data = []

    # Befehlsblock zusammenstellen
    command_block = [0xFF, 0xFF, modbus_cmd, subcmd] + data

    # In ASCII-Hex konvertieren
    ascii_hex = bytes_to_ascii_hex(command_block)

    # LRC auf den ASCII-String berechnen
    ascii_bytes = ascii_hex.encode('ascii')
    lrc = calculate_lrc(ascii_bytes)
    lrc_hex = f'{lrc:02X}'

    # Kompletten Befehl zusammensetzen
    full_command = f':{ascii_hex}{lrc_hex}\r\n'

    return full_command


def create_start_program_command(gender=0, oscillate=0, rotate=0, pulsate=0,
                                  pumpstate=3, nozzlestate=3, temperature=3):
    """
    Erstellt einen START_PROGRAM-Befehl

    Args:
        gender: 0=Lady, 1=Gent
        oscillate: 0=Off, 1=On
        rotate: 0=Off, 1=On
        pulsate: 0=Off, 1=On
        pumpstate: Wasserdruck 0-5
        nozzlestate: Düsenposition 0-5
        temperature: Temperatur 0-5

    Returns:
        Vollständiger Befehl als String
    """
    # Bitmask berechnen
    bitmask = ((gender + 1) & 0x03) | \
              ((oscillate & 0x01) << 2) | \
              ((rotate & 0x01) << 3) | \
              ((pulsate & 0x01) << 4)

    data = [bitmask, pumpstate, nozzlestate, temperature]

    return create_command(ModbusCommand.WRITE_COIL, VicCommand.START_PROGRAM, data)


def create_write_coil_command(subcmd, data=None):
    """Wrapper für Write Coil Befehle"""
    return create_command(ModbusCommand.WRITE_COIL, subcmd, data or [])


# Beispiel-Funktionen für häufig verwendete Befehle

def cmd_break_program():
    """Programm abbrechen"""
    return create_write_coil_command(VicCommand.BREAK_PROGRAM)


def cmd_set_nozzle_position(position):
    """Düsenposition setzen (0-5)"""
    return create_write_coil_command(VicCommand.SET_NOZZLE_POSITION, [position])


def cmd_set_temperature(temp):
    """Temperatur setzen (0-5)"""
    return create_write_coil_command(VicCommand.SET_TEMP, [temp])


def cmd_set_water_pump(pressure):
    """Wasserpumpe setzen (0-5)"""
    return create_write_coil_command(VicCommand.SET_WATER_PUMP, [pressure])


def cmd_toggle_oscillation():
    """Oszillation umschalten"""
    return create_write_coil_command(VicCommand.TOGGLE_OSCILLATION)


def cmd_toggle_rotation():
    """Rotation umschalten"""
    return create_write_coil_command(VicCommand.TOGGLE_ROTATION)


def cmd_toggle_pulsating():
    """Pulsation umschalten"""
    return create_write_coil_command(VicCommand.TOGGLE_PULSATING)


def cmd_toggle_hw():
    """Warmwasser umschalten"""
    return create_write_coil_command(VicCommand.TOGGLE_HW)


def cmd_toggle_buzzer():
    """Buzzer umschalten"""
    return create_write_coil_command(VicCommand.TOGGLE_BUZZER)


def cmd_nozzle_maintenance():
    """Düsen-Wartungsmodus aktivieren"""
    return create_write_coil_command(VicCommand.NOZZLE_MAINTENANCE, [0x01, 0x00])


def print_command_info(description, command):
    """Hilfsfunktion zur formatierten Ausgabe"""
    print(f'\n{description}')
    print(f'  Befehl: {command.strip()}')
    print(f'  Hex:    {" ".join(f"{ord(c):02X}" for c in command.strip())}')
    print(f'  Länge:  {len(command.strip())} Bytes')


# Hauptprogramm - Demonstriert alle Befehle
if __name__ == '__main__':
    print('=' * 70)
    print('ViClean DuschWC Command Generator - Proof of Concept')
    print('=' * 70)

    # Beispiel 1: Programm starten
    print('\n--- BEISPIEL 1: Programm starten ---')
    cmd = create_start_program_command(
        gender=0,        # Lady
        oscillate=1,     # On
        rotate=0,        # Off
        pulsate=1,       # On
        pumpstate=3,     # Stufe 3
        nozzlestate=3,   # Stufe 3
        temperature=3    # Stufe 3
    )
    print_command_info('START_PROGRAM (Lady, Osc+Puls, alle Stufe 3)', cmd)

    # Beispiel 2: Temperatur setzen
    print('\n--- BEISPIEL 2: Einfache Befehle ---')
    print_command_info('Temperatur auf Stufe 4 setzen', cmd_set_temperature(4))
    print_command_info('Wasserdruck auf Stufe 2 setzen', cmd_set_water_pump(2))
    print_command_info('Düsenposition auf Stufe 3 setzen', cmd_set_nozzle_position(3))

    # Beispiel 3: Toggle-Befehle
    print('\n--- BEISPIEL 3: Toggle-Befehle ---')
    print_command_info('Oszillation umschalten', cmd_toggle_oscillation())
    print_command_info('Rotation umschalten', cmd_toggle_rotation())
    print_command_info('Pulsation umschalten', cmd_toggle_pulsating())
    print_command_info('Warmwasser umschalten', cmd_toggle_hw())

    # Beispiel 4: Programm-Kontrolle
    print('\n--- BEISPIEL 4: Programm-Kontrolle ---')
    print_command_info('Programm abbrechen', cmd_break_program())
    print_command_info('Düsen-Wartung', cmd_nozzle_maintenance())

    # Beispiel 5: Verschiedene Programm-Kombinationen
    print('\n--- BEISPIEL 5: Verschiedene Programm-Modi ---')

    cmd = create_start_program_command(
        gender=1,        # Gent
        oscillate=0,
        rotate=1,
        pulsate=0,
        pumpstate=5,
        nozzlestate=4,
        temperature=2
    )
    print_command_info('START_PROGRAM (Gent, nur Rotation)', cmd)

    cmd = create_start_program_command(
        gender=0,        # Lady
        oscillate=1,
        rotate=1,
        pulsate=1,
        pumpstate=5,
        nozzlestate=5,
        temperature=5
    )
    print_command_info('START_PROGRAM (Lady, alle Features, Max)', cmd)

    print('\n' + '=' * 70)
    print('ESP32-Hinweis: Befehle über BLE NUS RX Characteristic senden')
    print('UUID: 6E400002-B5A3-F393-E0A9-E50E24DCCA9E')
    print('=' * 70)
    print()
