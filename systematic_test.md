# Systematischer Test-Plan

## Hypothese: Gerät antwortet nur bei Zustandsänderung

### Test 1: TEMP (bekanntermaßen funktioniert)
```
temp 0   → Erwarte RX
temp 0   → Erwarte RX oder KEIN RX?
temp 3   → Erwarte RX
temp 3   → Erwarte RX oder KEIN RX?
```

**Ergebnis:** _________________

---

### Test 2: OSC (gibt kein RX)
```
osc      → Warte 2 Sek → RX?
osc      → Warte 2 Sek → RX?
osc      → Warte 2 Sek → RX?
```

**Ergebnis:** _________________

---

### Test 3: Timing-Test
Sende OSC mit längerer Wartezeit:
```
osc
(warte 5 Sekunden)
Kommt RX?
```

**Ergebnis:** _________________

---

### Test 4: START vereinfacht
Versuche START ohne Oszillation (einfacher):
```
raw :FFFF058E010303037A
```
(Das ist: gender=0, osc=0, rot=0, puls=0, pump=3, nozzle=3, temp=3)

**Ergebnis:** _________________

---

### Test 5: START mit minimalen Parametern
```
raw :FFFF058E0100000008
```
(Das ist: gender=0, alle=0, pump=0, nozzle=0, temp=0)

**Ergebnis:** _________________

---

## Alternative Hypothese: START braucht READ statt WRITE?

Die Responses zeigen Adresse `0145` statt `FFFF`.
Vielleicht müssen wir nach START einen **READ**-Befehl senden?

### Test 6: Read Register nach START
```
start
(warteauf Trennung)
(nach Wiederverbindung:)
raw :FFFF03000000FC
```
(Das ist: Read Register Command 0x03)

**Ergebnis:** _________________
