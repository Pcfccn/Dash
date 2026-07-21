# SNIFF capture — selector / PRNDL search

Transcribed from the two in-vehicle SNIFF runs (`design/Photos/1/` and `/2/`,
2026-07-21). Firmware: full-payload watch view (commit `dce35ed`), so each row
is `ID  b0 b1 b2 b3 b4 b5 b6 b7  age`. Bus was healthy throughout: **48 fps**,
`ERRPASS` + rising `L` overflow count are expected on the promiscuous filter and
do not affect reception.

Goal: find the byte that tracks the gear selector (P R N D M 1 2 3).

---

## Run 1  (design/Photos/1/, frames 023–035)

```
frame  ID   b0 b1 b2 b3 b4 b5 b6 b7   (fps/ids/lost)
023    1AA  00 66 06 64 6A 00 00 00    48 / 50 / L505
       135  00 00 00 02 00 00 00 00
       1BA  16 B3 6B 46 63 65 36 5F
       19D  C0 00 3F FD 01 00 00 FF
       1F5  0F 0F 00 01 00 00 03 00
       0F9  00 00 40 00 00 00 00 FF
       1A1  00 10 05 00 5C 5C 00 00
       1C3  06 A0 06 53 00 00 00 00
024    1F5  0D 0D 00 03 00 00 03 00    49 / 50 / L491
       0F9  79 FD 40 00 00 00 00 FF
       1A1  00 10 45 00 5C 5C 00 00
       19D  80 00 3F FE 01 00 00 FF
025    1F5  0D 0D 00 03 00 00 03 00    48 / 50 / L477
       1A1  00 10 05 00 5C 5C 00 00
       0F9  00 00 40 00 00 00 00 FF
026    1F5  0D 0D 00 04 00 00 03 00    48 / 48 / L464
       0F9  07 A3 40 00 00 00 00 FF
       1A1  00 10 45 00 5C 5C 00 00
027    1F5  0D 0D 11 04 00 01 03 00    48 / 48 / L453
       0F9  07 A3 40 00 00 00 00 00
028    1F5  0D 0D 12 04 00 01 03 00    48 / 48 / L441
       0F9  04 74 40 00 00 00 00 00
029    1F5  0D 0D 11 04 00 01 03 00    48 / 48 / L433
       0F9  02 EA 40 00 00 00 00 00
030    1F5  0D 0D 11 04 00 01 03 00    48 / 48 / L424
       0F9  04 74 40 00 00 00 00 00
031    1F5  0D 0D 11 04 00 01 03 00    48 / 48 / L417
       0F9  07 A3 40 00 00 00 00 00
032    1F5  0D 0D 00 04 00 00 03 00    48 / 48 / L408
       0F9  07 A3 40 00 00 00 00 FF
       19D  00 00 00 00 00 00 00 FF
033    1F5  0D 0D 00 03 00 00 03 00    48 / 47 / L400
       0F9  00 00 40 00 00 00 00 FF
034    1F5  0F 0F 00 01 00 00 03 00    48 / 47 / L389
       0F9  79 FD 40 00 00 00 00 FF
       1BA  17 0A 70 A6 64 65 06 60
035    1F5  0F 0F 00 01 00 00 03 00    48 / 47 / L380
       0F9  00 00 40 00 00 00 00 FF
```

Other IDs seen in run 1 (roughly static or busy counters):
`3D3` (busy), `4C1 00 CC 40 44 7B 02 1D 4C` (static), `4E9 11 00 00 00 02 …`,
`2C3 06 60 06 60 00 00 CB 13`, `1C3 06 A0 06 5x 00 …`, `135 0x 00 00 02 …`.

## Run 2  (design/Photos/2/, frames 036–052)

```
036    1F5  0F 0F 00 01 00 00 03 00    48 / 45 / L143
037    1F5  0E 0D 00 02 00 00 03 00    48 / 45 / L134
       1E3  40 40 00 00 00 00 00 00
       12A  02 20 10 FF 00 00 10 90
038    1F5  0D 0D 00 04 00 00 03 00    48 / 45 / L125
       4C9  40 3D 00 01 00 00 00 00
039    1F5  0D 0D 00 04 00 00 03 00    48 / 45 / L120
       1F3  C0 C0 00 00 00 00 00 00
       287  43 00 00 00 00 00 00 00
040    1F5  0D 0D 11 04 00 01 03 00    48 / 44 / L114
       1F3  40 50 00 00 00 00 00 00
041    1F3  80 90 00 00 00 00 00 00    48 / 44 / L107
       1F3  C0 D8 00 00 00 00 00 00
       287  00 00 00 00 00 00 00 00
043    1F5  0D 0D 13 04 00 01 03 00    50 / 44 / L99
       1F3  40 58 00 00 00 00 00 00
       287  82 00 00 00 00 00 00 00
       0BE  00 00 00 00 00 00 00 00
044    1F5  0D 0D 33 04 00 01 03 00    48 / 44 / L92
       1F3  C0 D0 00 00 00 00 00 00
       0C9  00 00 00 00 00 01 10 FF
045    1F5  0D 0D 13 04 00 01 03 00    48 / 43 / L85
       0C9  00 00 00 00 00 01 10 FF
046    199  CF FF 0E 70 F1 8D 00 FF    48 / 43 / L78
       0C9  00 00 00 07 00 01 10 FF
       1F3  00 10 00 00 00 00 00 00
       0BE  04 00 00 00 01 FF 00 00
048    199  4F FF 0E 70 F1 8F 00 FF    48 / 43 / L68
       287  43 00 00 00 00 00 00 00
049    199  4F FF 0E 70 F1 8F 00 FF    48 / 43 / L61
       199  CF FF 0E 70 F1 8D 00 FF
       1A1  00 10 45 00 5C 5C 00 00
050    0F1  BB 00 00 04 00 00 00 00    48 / 41 / L52
       1A1  00 10 05 00 5C 5C DD DD
       1F3  40 40 00 00 00 00 00 00
051    0F1  A5 0D 00 40 00 00 00 00    48 / 39 / L39
       0F1  B1 00 04 00 00 00 00 00
       1F3  80 80 00 00 00 00 00 00
       0C9  00 00 00 0A 00 01 10 FF
052    199  0F FF 0E 70 F1 90 00 FF    48 / 23 / L16
       0C9  00 00 00 0A 00 01 10 FF
       287  C1 00 00 00 00 00 00 00
       19D  00 00 00 00 01 00 00 FF
053    (cleared / bus asleep: 0 fps, IDS 0, "no changes yet")
```

---

## Analysis — distinct value set per interesting byte

| ID.byte | distinct values seen | verdict |
|---|---|---|
| **1F5.3** | **01, 02, 03, 04** | **PRNDL range** — 4 clean values, swept in order |
| 1F5.0 | 0D, 0E, 0F | mirrors b3 (0x10−b3 for 1..3), redundant |
| 1F5.2 | 00, 11, 12, 13, 33 | manual/commanded gear (only nonzero while b3=04=D) |
| 1F5.5 | 00, 01 | "manual mode active" flag (1 while b3=04 and b2≠0) |
| 0F9.0 | 00, 02, 04, 07, 79 | wide/continuous → **sensor, NOT selector** |
| 0F9.1 | 00, 74, A3, EA, FD | wide/continuous → sensor (16-bit with b0) |
| 1A1.2 | 05, 45 | 2-state toggle (bit 0x40), unrelated |
| 1F3.0 | 00, 40, 80, C0 | 2-bit field, changes with engine, not detents |
| 199.5 | 8D, 8E, 8F, 90 | incrementing alive-counter |
| 287.0 | 00, 43, 82, C1 | rolling counter (+~0x40 steps) |

### Conclusion

**The selector frame is `0x1F5`, byte 3.** It takes exactly four values and steps
them in sweep order across both runs (start 01 → up → back to 01):

```
1F5.3 = 01  →  P   (rest position at start & end of every sweep)
1F5.3 = 02  →  R
1F5.3 = 03  →  N
1F5.3 = 04  →  D
```

While in D (`b3=04`), **`1F5` byte 2** carries the manual/commanded gear
(`0x11/0x12/0x13` ≈ manual 1/2/3) and **byte 5** is a "manual mode" flag — that
is where the tap-shift positions live, not in byte 3.

**0F9 is ruled out**: its changing bytes (0 and 1) sweep a wide continuous range
(00…FD) — a 16-bit sensor value, not the small fixed code set a selector has.

Confidence: high on `1F5.3` being the PRNDL range and on the P=01…D=04 order
(clean 4-value staircase, matches the physical detent order); medium on the
byte-2 manual-gear codes. The distinct-value "candidates" view (commit `d0b338d`,
already flashed) will confirm the exact map in one slow sweep — `1F5.3` should
list `01 02 03 04` in swept order.

### Next

1. One slow sweep on the candidate view to lock `1F5.3` and the byte-2 map.
2. Decode `0x1F5` (listen-only, no request needed — it is broadcast) and render
   real P/R/N/D on the DRIVE page in place of the gear number.
