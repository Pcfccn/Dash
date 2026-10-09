# Road-test session — 2026-08-23 (26 photos)

Firmware flashed: `1d56fd2` (fast RPM band, MAP paired with RPM, 0x1BA oil-pressure
broadcast candidate, PRNDL from 0x1F5, oil-pressure A/B test on DIAG).
Engine warm, idling / short move in the yard. User's verbal summary:
**oil weird — drops too low; boost & RPM ok; speed lags ~10 s and reads ~12 % high.**

## DRIVE page readings (photos 19–26)

| # | Gear | Speed km/h | RPM | Boost | Coolant | OIL °C | ATF | OIL P bar |
|---|------|-----------|-----|-------|---------|--------|-----|-----------|
| 19 | D1 | 0  | 758  | 0.0 | 75 | 114 | 70 | 2.1 |
| 20 | D1 | 0  | 851  | 0.0 | 75 | 114 | 70 | 0.4 (red) |
| 21 | D1 | 0  | 740  | 0.0 | 75 | 114 | 70 | 2.0 |
| 22 | D1 | 12 | 763  | 0.0 | 75 | 113 | 73 | 2.5 |
| 23 | D1 | 12 | 739  | 0.0 | 75 | 113 | 73 | 0 |
| 24 | D1 | 0  | 739  | 0.0 | 81 | 100 | 71 | 0.2 (red) |
| 25 | D1 | 0  | 739  | 0.0 | 81 | 100 | 71 | 0.1 (red) |
| 26 | D5 | 71 | 1599 | 0.0 | 78 | 89  | 68 | 0.2 (red) |

## DIAG page (photos 17–18)

BATT 13.1 V · IAT 36 °C · LOAD 20 % · RAIL 376 then 386 bar · MIL/CHECK ENGINE OFF ·
NO STORED CODES. (The oil-pressure A/B raw-byte line described in
`docs/oil-pressure-test.md` was not legible in these two dark shots.)

## Findings

### 1. OIL P (oil pressure) — candidate 0x1BA[3] is NOT oil pressure — REJECT
Erratic across the set: 2.1 / 0.4 / 2.0 / 2.5 / 0 / 0.2 / 0.1 / 0.2 bar. It drops to
0.1–0.4 bar at warm idle (739–851 rpm) and **stays 0.2 bar at 1599 rpm / 71 km/h under
way** — real oil pressure rises with RPM and would be 3–5 bar there. No RPM correlation,
no lag-on-decay. This is the "oil drops too low" complaint. Confirms the 2026-07-24
suspicion in `docs/oil-pressure-test.md`. **Action:** stop trusting `0x1BA[3]`; test
candidate #2 `0x0C9[2]` or keep hunting via SNIFF, then per the doc delete the loser's
RX branch + acceptance filter and drop `StdFiltersNbr` back.

### 2. SPEED — ~10 s lag + ~12 % high
PID `0x0D` ("A km/h", direct byte). The lag = SPEED sits on a slow poll band; it should
move to the fast band next to RPM/MAP. The +12 % is a separate scaling/source question
(0x0D is km/h directly; OEM speed can run optimistic vs GPS). Needs a GPS reference run
before changing any formula — don't "fix" the 12 % blind, it may be genuine ECM behaviour.

### 3. OIL temp — plausible magnitude, trend to verify
OIL reads 114 → 113 → 100 → 89 °C while coolant rises 75 → 81. Oil above coolant right
after load, cooling at idle, is physically possible, so not obviously broken — but PID
`0x5C` was previously logged as *unsupported* on this ECM (see obd-did-discovery), so
confirm whether this is a real modelled value or a decode artifact before trusting it.

### 4. Reads OK (no action)
RPM (758–1599 tracks), BOOST 0.0 (light cruise, plausible), COOLANT 75–81, ATF 68–73,
RAIL 376–386 @ idle, LOAD 20 %, IAT 36, BATT 13.1, MIL off / no codes, GEAR = engaged
ratio (D1/D5, matches known 0x199A limitation).

## SNIFF / ANALOG candidate frames (photos 1–16, passive change detector)

Hunting PRNDL / soot / DPF-∆P sources. Bytes that ramp like analog signals (byte.MIN>MAX):
`3D3.5` `2C3.1` `3D1.5` `3E9.1` `3F9.2` `3F9.1` `199.1` `0F9.5`.
Structured/state-type frames seen changing: `0F1` `0BE` `19D` `287` `1E1` `1F3` `191`
`0C7` `199`. Not yet correlated to a specific control — needs the operate-one-control
method (blip throttle / move selector while watching which byte follows).
