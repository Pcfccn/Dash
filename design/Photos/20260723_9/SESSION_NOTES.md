# Сессия 2026-07-23, утро (~10:10) — расшифровка фото

Стенд с машиной. 14 фото. Папка `design/Photos/20260724_9` на момент съёмки пуста (создана позже).
Фото сделаны **до** коммитов `49c65be` (fix false 0.0 OIL P) и `e9b38e5` (RAIL в bar вместо MPa).

---

## DRIVE page

### Фото 7 (625) — двигатель работает, RPM=1056
```
GEAR   P         BOOST  0.0 bar
       0 km/h    RPM    1056
COOLANT  19 °C   OIL    17 °C
ATF      16 °C   OIL P  -- bar   ← NaN, не читается
Статус: NOMINAL
```

### Фото 12 (630) — двигатель заглушен, WARN
```
GEAR   P         BOOST  0.0 bar
       0 km/h    RPM    0
COOLANT  16 °C   OIL    16 °C
ATF      16 °C   OIL P  0.0 bar  ← КРАСНЫЙ, CRIT alert
Статус: WARN 1
```

### Фото 14 (632) — двигатель заглушен, CHECK
```
GEAR   P         BOOST  0.0 bar
       0 km/h    RPM    0
COOLANT  14 °C   OIL    13 °C
ATF      14 °C   OIL P  0.0 bar  ← КРАСНЫЙ, CRIT alert
Статус: CHECK 1
```

---

## DIAG page

### Фото 6 (624) — двигатель работает
```
MIL/CHECK ENGINE: OFF
NO STORED CODES  ✓
BATT   14.2 V    IAT   14 °C
LOAD   18 %      RAIL  38 MPa   ← MPa (ещё не исправлено на bar)
Отладка: GR 01 OILP -- NRC 22/31 / PID probe: no reply yet
```

### Фото 9 (627) — двигатель не запущен (ключ вставлен)
```
MIL/CHECK ENGINE: OFF
NO STORED CODES  ✓
BATT   12.0 V    IAT   21 °C
LOAD   0 %       RAIL  0 MPa
Отладка: GR 01 OILP 00=0.00 NRC 22/31 / PID probe: no reply yet
```

### Фото 10 (628) — двигатель не запущен
```
MIL/CHECK ENGINE: OFF
NO STORED CODES  ✓
BATT   12.0 V    IAT   21 °C
LOAD   0 %       RAIL  1 MPa    ← остаточное давление в рейке
Отладка: GR 01 OILP 00=0.00 NRC 22/31 / PID probe: no reply yet
```

---

## DPF page

### Фото 11 (629)
```
SOOT: 8%
REGENERATION: Inactive
  "No regeneration request from ECM"
  70% warning / 85% regen threshold
EGT        -- °C      dP DPF    0.0 kPa
SINCE REGEN -- km     EGR T°    -- °C
Статус: NOMINAL
```

---

## SNIFF page (STATE + ANALOG)

### Фото 1 (619) — CAN LOST 1, 64 fps, IDS 40, L179
**STATE** (ID.Byte = N Values):
```
0BE.0   4   0C040800
0BE.5   4   FDFFFE00
0C9.3   4   0D070A00
191.1   5   AEADB0AFB1
191.3   5   AEADB0AFB1
287.0   4   8200C143
```
**ANALOG** (ID.Byte MIN>MAX=NOW):
```
1E1.1   00>FF =00
0BE.5   00>FF =FF
19D.3   00>FF =FE
3D3.5   00>FF =00
1AA.2   06>F6 =16
```

### Фото 2 (620) — NOMINAL, 64 fps, IDS 40, L152
**STATE**:
```
1F3.0   4   C0800040
1F3.1   4   C0800040
191.1   5   AEADB0AFB1
191.3   5   AEADB0AFB1
1AA.5   5   E0D000F010
1C3.1   5   AEADB0AFB1
```
**ANALOG**:
```
1E1.1   00>FF =00
0BE.5   00>FF =FF
19D.3   00>FF =FD
3D3.5   00>FF =00
1AA.2   06>F6 =06
```

### Фото 3 (621) — NOMINAL, 61 fps, IDS 39, L146, OBD paused
**STATE**:
```
1E1.1   4   FDFEFF00
1E1.2   4   03020100
1F3.0   4   C0800040
1F3.1   4   C0800040
191.1   5   AEADB0AFB1
191.3   5   AEADB0AFB1
```
**ANALOG**:
```
1E1.1   00>FF =FD
0BE.5   00>FF =FE
19D.3   00>FF =00
3D3.5   00>FF =00
1AA.2   06>F6 =06
```

### Фото 4 (622) — CAN LOST, 32 fps, IDS 36, L115
**STATE**:
```
287.0   4   8200C143
0F1.0   4   A5B1998D
0BE.0   4   0C040800
0BE.5   4   FDFFFE00
0C9.3   4   0D070A00
191.1   4   AEADB0AF
```
**ANALOG**:
```
0BE.5   00>FF =FD
1AA.5   00>F0 =E0
1BA.3   06>F6 =F6
1BA.6   09>F9 =F9
287.0   00>C1 =82
```

### Фото 5 (623) — CAN LOST, 48 fps, IDS 34, L110, OBD paused
**STATE**:
```
1E1.1   3   FDFEFF
1E1.2   3   030201
0BE.0   4   0C040800
0BE.5   4   FDFFFE00
0C9.3   4   0D070A00
191.5   5   B0AEAFADB1
```
**ANALOG**:
```
0BE.5   00>FF =FD
1AA.5   00>F0 =E0
1BA.3   06>F6 =D6
1BA.6   09>F9 =D9
287.0   00>C1 =00
```

### Фото 8 (626) — NOMINAL, 61 fps, IDS 26, L61, OBD paused
**STATE**:
```
199.0   3   8F4FCF
199.5   3   8E8F8D
19D.0   3   8040C0
19D.3   3   FEFFFD
0F1.0   4   A5B1998D
0BE.0   4   0C040008
```
**ANALOG**:
```
0BE.5   00>FF =00
0C9.7   00>FF =FF
1E1.1   00>FF =FF
287.0   00>C1 =00
1F3.0   00>C0 =40
```

### Фото 13 (631) — CAN LOST, 0 fps, IDS 0, L0 (первый вход на страницу)
```
no state bytes yet
no analog bytes yet
```
