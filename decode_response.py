#!/usr/bin/env python3
"""
ViClean Response Decoder
Dekodiert die Hex-Antworten vom ViClean-Gerät

Verwendung:
  python3 decode_response.py "3A 30 31 34 35 38 35 30 30 30 33 30 30 32 32 30 45 0D 0A"
"""

import sys

def decode_response(hex_string):
    """Dekodiert ViClean Response"""
    # Entferne Leerzeichen und konvertiere zu Bytes
    hex_bytes = hex_string.replace(" ", "")

    # Konvertiere Hex zu ASCII
    ascii_chars = []
    for i in range(0, len(hex_bytes), 2):
        hex_byte = hex_bytes[i:i+2]
        ascii_char = chr(int(hex_byte, 16))
        ascii_chars.append(ascii_char)

    ascii_string = ''.join(ascii_chars)

    print("="*60)
    print("ViClean Response Decoder")
    print("="*60)
    print(f"Hex Input:  {hex_string}")
    print(f"ASCII:      {repr(ascii_string)}")
    print(f"Readable:   {ascii_string.strip()}")
    print("="*60)

    # Parse Modbus Response
    if ascii_string.startswith(':'):
        parts = ascii_string.strip().replace(':', '').replace('\r', '').replace('\n', '')

        if len(parts) >= 4:
            addr = parts[0:4]  # Adresse
            cmd = parts[4:6] if len(parts) > 4 else "??"

            print(f"\nModbus Response:")
            print(f"  Adresse:     {addr}")
            print(f"  Befehl:      0x{cmd}")

            # Befehlsnamen
            cmd_names = {
                "05": "Write Coil (0x05)",
                "85": "Set Temperature (0x85)",
                "7E": "Set Water Pump (0x7E)",
                "81": "Set Nozzle Position (0x81)",
                "8E": "Start Program (0x8E)",
                "8F": "Break Program (0x8F)",
                "88": "Toggle Oscillation (0x88)",
                "89": "Toggle Rotation (0x89)",
                "8D": "Toggle Pulsating (0x8D)",
                "8A": "Toggle Hot Water (0x8A)",
                "65": "Toggle Buzzer (0x65)"
            }

            if cmd in cmd_names:
                print(f"  Name:        {cmd_names[cmd]}")

            if len(parts) > 6:
                data = parts[6:-2]  # Daten (ohne LRC)
                lrc = parts[-2:]    # LRC Checksum
                print(f"  Daten:       {data}")
                print(f"  LRC:         {lrc}")

        print("="*60)

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Verwendung: python3 decode_response.py <hex_string>")
        print("\nBeispiel:")
        print('  python3 decode_response.py "3A 30 31 34 35 38 35 30 30 30 33 30 30 32 32 30 45 0D 0A"')
        print("\nOder direkt einen Response eingeben:")
        hex_input = input("Hex Response: ")
        if hex_input:
            decode_response(hex_input)
    else:
        hex_input = ' '.join(sys.argv[1:])
        decode_response(hex_input)
