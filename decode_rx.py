#!/usr/bin/env python3
"""
Dekodiert die RX-Antworten vom Gerät
"""

def decode_rx(hex_bytes):
    """Dekodiert RX-Bytes zu ASCII"""
    ascii_str = ''.join([chr(int(b, 16)) for b in hex_bytes.split()])
    print(f"RX Bytes: {hex_bytes}")
    print(f"Als ASCII: {ascii_str}")
    print(f"Ohne :\\r\\n: {ascii_str.strip().replace(':', '').replace('\\r', '').replace('\\n', '')}")
    print()

print("=== TEMP 3 Response ===")
decode_rx("3A 30 31 34 35 38 35 30 30 30 33 30 30 32 32 30 45 0D 0A")

print("=== PUMP 3 Response ===")
decode_rx("3A 30 31 34 35 37 45 30 30 30 33 30 30 30 30 37 31 0D 0A")

print("=== NOZZLE 3 Response ===")
decode_rx("3A 30 31 34 35 38 31 30 30 30 33 30 30 30 30 30 41 0D 0A")

print("=== NOZZLE 1 Response ===")
decode_rx("3A 30 31 34 35 38 31 30 30 30 31 46 46 46 45 30 42 0D 0A")
