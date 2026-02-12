#!/usr/bin/env python3
"""
Verifiziert ob unsere gesendeten Befehle korrekt sind
"""

def calculate_lrc(data_bytes):
    lrc = 0
    for byte in data_bytes:
        lrc ^= byte
    return lrc

def verify_command(name, sent_cmd):
    """Verifiziert einen gesendeten Befehl"""
    print(f"\n{'='*60}")
    print(f"Befehl: {name}")
    print(f"Gesendet: {sent_cmd}")

    # Entferne : und \r\n
    cmd = sent_cmd.strip().replace(':', '').replace('\r', '').replace('\n', '')

    # Trenne LRC ab (letzte 2 Zeichen)
    if len(cmd) < 2:
        print("FEHLER: Befehl zu kurz!")
        return False

    data_part = cmd[:-2]
    lrc_sent = cmd[-2:]

    # Berechne LRC
    lrc_calc = calculate_lrc(data_part.encode('ascii'))
    lrc_calc_hex = f'{lrc_calc:02X}'

    print(f"Daten: {data_part}")
    print(f"LRC gesendet: {lrc_sent}")
    print(f"LRC berechnet: {lrc_calc_hex}")

    if lrc_sent == lrc_calc_hex:
        print("✓ LRC KORREKT!")

        # Parse Befehl
        if len(data_part) >= 8:
            addr = data_part[0:4]
            cmd_code = data_part[4:6]
            subcmd = data_part[6:8]
            params = data_part[8:] if len(data_part) > 8 else ""

            print(f"\nStruktur:")
            print(f"  Adresse: {addr} (soll: FFFF)")
            print(f"  Command: {cmd_code} (soll: 05 für Write Coil)")
            print(f"  Subcmd:  {subcmd}")
            print(f"  Params:  {params}")

            if addr != "FFFF":
                print("  ⚠️ WARNUNG: Adresse nicht FFFF!")
            if cmd_code != "05":
                print("  ⚠️ WARNUNG: Command nicht 05 (Write Coil)!")

        return True
    else:
        print("✗ LRC FALSCH!")
        return False

# Teste die Befehle aus deinem Log
print("VERIFIZIERUNG DER GESENDETEN BEFEHLE")
print("="*60)

# Befehle die RX bekommen haben
print("\n\n### BEFEHLE MIT RX (funktionieren) ###")
verify_command("BUZZ (AN)", ":FFFF056506\r\n")
verify_command("PUMP 5", ":FFFF057E0572\r\n")
verify_command("NOZZLE 5", ":FFFF05810509\r\n")
verify_command("TEMP 0", ":FFFF05850008\r\n")
verify_command("PULS", ":FFFF058D79\r\n")
verify_command("STOP", ":FFFF058F7B\r\n")

# Befehle ohne RX
print("\n\n### BEFEHLE OHNE RX (Problem?) ###")
verify_command("PUMP 0", ":FFFF057E0077\r\n")
verify_command("OSC", ":FFFF058805\r\n")
verify_command("ROT", ":FFFF058904\r\n")

# START Befehl
print("\n\n### PROBLEMATISCHER BEFEHL ###")
verify_command("START", ":FFFF058E050303037E\r\n")

# Analysiere was bei START anders sein könnte
print("\n\n" + "="*60)
print("ANALYSE: START-Befehl")
print("="*60)
print("START sendet: FFFF 05 8E 05030303")
print("             Addr Cmd Sub  Params")
print()
print("Params decodiert:")
print("  05 = Bitmask (0x05 = 0000 0101)")
print("       Bit 0-1: Gender = 01 (0+1 = 1 = Lady+1?)")
print("       Bit 2:   Oscillate = 1 ✓")
print("       Bit 3:   Rotate = 0 ✓")
print("       Bit 4:   Pulsate = 0 ✓")
print("  03 = Pumpstate (3)")
print("  03 = Nozzlestate (3)")
print("  03 = Temperature (3)")
print()
print("Das sieht korrekt aus laut Java-Code!")
print()
print("ABER: Könnte es sein, dass START ein anderes")
print("      Response-Format hat oder länger braucht?")
