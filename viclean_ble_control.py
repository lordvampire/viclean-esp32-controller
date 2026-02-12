#!/usr/bin/env python3
"""
ViClean BLE Controller - Direkt vom Laptop steuern

Erfordert: Python 3.7+ und Bleak (BLE Bibliothek)
Installation: pip install bleak

Verwendung:
    python3 viclean_ble_control.py

Dann interaktive Befehle eingeben (siehe 'help')
"""

import asyncio
import sys
from bleak import BleakClient, BleakScanner

# ViClean BLE UUIDs
SERVICE_UUID = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
RX_CHAR_UUID = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"  # Write
TX_CHAR_UUID = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"  # Notify

# Befehlscodes
CMD_START_PROGRAM = 0x8E
CMD_BREAK_PROGRAM = 0x8F
CMD_SET_NOZZLE_POSITION = 0x81
CMD_SET_TEMP = 0x85
CMD_SET_WATER_PUMP = 0x7E
CMD_TOGGLE_OSCILLATION = 0x88
CMD_TOGGLE_ROTATION = 0x89
CMD_TOGGLE_PULSATING = 0x8D
CMD_TOGGLE_HW = 0x8A
CMD_TOGGLE_BUZZER = 0x65
MODBUS_WRITE_COIL = 0x05

client = None


def calculate_lrc(data_bytes):
    """Berechnet LRC Checksum"""
    lrc = 0
    for byte in data_bytes:
        lrc ^= byte
    return lrc


def create_command(modbus_cmd, subcmd, data=None):
    """Erstellt einen vollständigen ViClean-Befehl"""
    if data is None:
        data = []

    command_block = [0xFF, 0xFF, modbus_cmd, subcmd] + data
    ascii_hex = ''.join(f'{b:02X}' for b in command_block)
    ascii_bytes = ascii_hex.encode('ascii')
    lrc = calculate_lrc(ascii_bytes)
    lrc_hex = f'{lrc:02X}'
    full_command = f':{ascii_hex}{lrc_hex}\r\n'

    return full_command


async def notification_handler(sender, data):
    """Callback für empfangene Benachrichtigungen"""
    print(f"[RX] {' '.join(f'{b:02X}' for b in data)}")


async def send_command(command, description=""):
    """Sendet einen Befehl an ViClean"""
    global client
    if client is None or not client.is_connected:
        print("[FEHLER] Nicht verbunden!")
        return

    if description:
        print(f"[INFO] {description}")

    print(f"[TX] {command.strip()}")
    await client.write_gatt_char(RX_CHAR_UUID, command.encode())
    await asyncio.sleep(0.1)  # Kurze Pause zwischen Befehlen


async def cmd_set_temperature(temp):
    """Temperatur setzen (0-5)"""
    cmd = create_command(MODBUS_WRITE_COIL, CMD_SET_TEMP, [temp])
    await send_command(cmd, f"Setze Temperatur: {temp}")


async def cmd_set_water_pump(pressure):
    """Wasserpumpe setzen (0-5)"""
    cmd = create_command(MODBUS_WRITE_COIL, CMD_SET_WATER_PUMP, [pressure])
    await send_command(cmd, f"Setze Pumpe: {pressure}")


async def cmd_set_nozzle_position(position):
    """Düsenposition setzen (0-5)"""
    cmd = create_command(MODBUS_WRITE_COIL, CMD_SET_NOZZLE_POSITION, [position])
    await send_command(cmd, f"Setze Düse: {position}")


async def cmd_start_program(gender=0, oscillate=1, rotate=0, pulsate=0,
                            pumpstate=3, nozzlestate=3, temperature=3):
    """Programm starten"""
    bitmask = ((gender + 1) & 0x03) | \
              ((oscillate & 0x01) << 2) | \
              ((rotate & 0x01) << 3) | \
              ((pulsate & 0x01) << 4)

    data = [bitmask, pumpstate, nozzlestate, temperature]
    cmd = create_command(MODBUS_WRITE_COIL, CMD_START_PROGRAM, data)
    await send_command(cmd, "Starte Programm")


async def cmd_break_program():
    """Programm abbrechen"""
    cmd = create_command(MODBUS_WRITE_COIL, CMD_BREAK_PROGRAM)
    await send_command(cmd, "Stoppe Programm")


async def cmd_toggle(toggle_cmd, name):
    """Toggle-Befehl"""
    cmd = create_command(MODBUS_WRITE_COIL, toggle_cmd)
    await send_command(cmd, f"Toggle {name}")


async def process_command(cmd_str):
    """Verarbeitet Benutzereingabe"""
    parts = cmd_str.strip().lower().split()
    if not parts:
        return

    cmd = parts[0]

    if cmd in ['help', '?']:
        print("""
╔════════════════════════════════════════╗
║      ViClean BLE Befehle               ║
╠════════════════════════════════════════╣
║ temp <0-5>    - Temperatur setzen      ║
║ pump <0-5>    - Wasserpumpe setzen     ║
║ nozzle <0-5>  - Düsenposition setzen   ║
║ start         - Programm starten       ║
║ stop          - Programm stoppen       ║
║ osc           - Oszillation toggle     ║
║ rot           - Rotation toggle        ║
║ puls          - Pulsation toggle       ║
║ hw            - Warmwasser toggle      ║
║ buzz          - Buzzer toggle          ║
║ raw <cmd>     - Roher Befehl           ║
║ quit/exit     - Beenden                ║
║ help          - Diese Hilfe            ║
╚════════════════════════════════════════╝
        """)

    elif cmd == "temp" and len(parts) == 2:
        val = int(parts[1])
        if 0 <= val <= 5:
            await cmd_set_temperature(val)
        else:
            print("[FEHLER] Wert muss 0-5 sein")

    elif cmd == "pump" and len(parts) == 2:
        val = int(parts[1])
        if 0 <= val <= 5:
            await cmd_set_water_pump(val)
        else:
            print("[FEHLER] Wert muss 0-5 sein")

    elif cmd == "nozzle" and len(parts) == 2:
        val = int(parts[1])
        if 0 <= val <= 5:
            await cmd_set_nozzle_position(val)
        else:
            print("[FEHLER] Wert muss 0-5 sein")

    elif cmd == "start":
        await cmd_start_program()

    elif cmd == "stop":
        await cmd_break_program()

    elif cmd == "osc":
        await cmd_toggle(CMD_TOGGLE_OSCILLATION, "Oszillation")

    elif cmd == "rot":
        await cmd_toggle(CMD_TOGGLE_ROTATION, "Rotation")

    elif cmd == "puls":
        await cmd_toggle(CMD_TOGGLE_PULSATING, "Pulsation")

    elif cmd == "hw":
        await cmd_toggle(CMD_TOGGLE_HW, "Warmwasser")

    elif cmd == "buzz":
        await cmd_toggle(CMD_TOGGLE_BUZZER, "Buzzer")

    elif cmd == "raw" and len(parts) == 2:
        await send_command(parts[1], "Sende rohen Befehl")

    elif cmd in ['quit', 'exit']:
        return False

    else:
        print("[FEHLER] Unbekannter Befehl. Tippe 'help' für Hilfe.")

    return True


async def scan_for_viclean():
    """Scannt nach ViClean-Geräten"""
    print("[INFO] Scanne nach ViClean-Geräten...")

    devices = await BleakScanner.discover(timeout=5.0)

    viclean_devices = []
    for device in devices:
        if device.name and "viclean" in device.name.lower():
            viclean_devices.append(device)
            print(f"  Gefunden: {device.name} ({device.address})")

    if not viclean_devices:
        print("[FEHLER] Kein ViClean-Gerät gefunden!")
        return None

    # Verwende das erste gefundene Gerät
    return viclean_devices[0]


async def main():
    global client

    print("""
╔════════════════════════════════════════╗
║   ViClean BLE Controller (Laptop)      ║
║   Direkte BLE-Steuerung vom PC         ║
╚════════════════════════════════════════╝
    """)

    # Suche ViClean
    device = await scan_for_viclean()
    if device is None:
        return

    print(f"\n[INFO] Verbinde zu {device.name}...")

    try:
        async with BleakClient(device.address) as client_instance:
            client = client_instance

            # Aktiviere Benachrichtigungen
            await client.start_notify(TX_CHAR_UUID, notification_handler)
            print("[INFO] Verbunden und bereit!")
            print("[INFO] Tippe 'help' für Befehle\n")

            # Interaktive Schleife
            while True:
                try:
                    # In asyncio muss input() anders gehandhabt werden
                    cmd = await asyncio.get_event_loop().run_in_executor(
                        None, input, "viclean> "
                    )

                    if not await process_command(cmd):
                        break

                except KeyboardInterrupt:
                    print("\n[INFO] Unterbrochen")
                    break
                except Exception as e:
                    print(f"[FEHLER] {e}")

            await client.stop_notify(TX_CHAR_UUID)

    except Exception as e:
        print(f"[FEHLER] Verbindung fehlgeschlagen: {e}")
        print("\n[TIPP] Stelle sicher, dass:")
        print("  1. Bluetooth auf deinem Laptop aktiviert ist")
        print("  2. Das ViClean-Gerät eingeschaltet ist")
        print("  3. Du 'pip install bleak' ausgeführt hast")


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n[INFO] Beendet")
