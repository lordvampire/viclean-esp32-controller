# START-Befehl Test-Suite

Systematische Tests um herauszufinden was START braucht.

## Bekannte funktionierende Befehle (bekommen RX):
```
raw :FFFF0585030B          # TEMP 3 → RX ✓
raw :FFFF057E0374          # PUMP 3 → RX ✓
raw :FFFF0581030F          # NOZZLE 3 → RX ✓
raw :FFFF058F7B            # STOP → RX ✓
```

## START-Tests (4 Parameter - Standard)

### Test 1: Minimal (alle Werte 0)
```
raw :FFFF058E00000000
```
Bitmask=0, pump=0, nozzle=0, temp=0

### Test 2: Minimal (alle Werte 1)
```
raw :FFFF058E01010101
```
Bitmask=1, pump=1, nozzle=1, temp=1

### Test 3: Standard (Lady, HW, 3-3-3)
```
raw :FFFF058E0D030303
```
Bitmask=0x0D, pump=3, nozzle=3, temp=3

### Test 4: Nur Gender-Bits
```
raw :FFFF058E01000000
```
Bitmask=1 (nur gender), Rest 0

### Test 5: Nur Pump
```
raw :FFFF058E00030000
```
Bitmask=0, pump=3, Rest 0

## START-Tests (5 Parameter - wie TEST-Funktion)

### Test 6: 5 Parameter {1,1,1,1,3}
```
raw :FFFF058E0101010103
```

### Test 7: 5 Parameter {1,1,1,1,1}
```
raw :FFFF058E0101010101
```

### Test 8: 5 Parameter {0,0,0,0,0}
```
raw :FFFF058E0000000000
```

### Test 9: 5 Parameter {0x0D,3,3,3,1}
```
raw :FFFF058E0D03030301
```

## START-Tests (3 Parameter)

### Test 10: Nur 3 Parameter (ohne Bitmask)
```
raw :FFFF058E030303
```
pump=3, nozzle=3, temp=3 (OHNE Bitmask!)

## START-Tests (andere Reihenfolge)

### Test 11: Nozzle-Temp-Pump (statt Bitmask-Pump-Nozzle-Temp)
```
raw :FFFF058E030303
```

### Test 12: Mit führender 0
```
raw :FFFF058E000D030303
```
0, Bitmask, pump, nozzle, temp

## START-Tests (Command-Code Varianten)

### Test 13: Mit 0x06 (Write Register statt Write Coil)
```
raw :FFFF068E0D030303
```

### Test 14: START CW NOZZLE (0x70 = 112)
```
raw :FFFF057003
```

### Test 15: START CCW NOZZLE (0x71 = 113)
```
raw :FFFF057103
```

### Test 16: START CW WIGGLE (0x72 = 114)
```
raw :FFFF057203
```

## STOP zum Vergleich (funktioniert!)
```
raw :FFFF058F          # STOP ohne Parameter
```

## Notizen
- Alle RAW-Befehle brauchen korrekten LRC (wird automatisch berechnet)
- Wenn ein Befehl RX bekommt → ERFOLG!
- Wenn physisch etwas passiert → JACKPOT!
