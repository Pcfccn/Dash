# GM-enhanced DIDs (mode 22) for the Colorado 2.8 / E98

These are the manufacturer-specific parameters that apps like Car Scanner,
Torque and OBD Fusion read — not standard J1979 PIDs, but **UDS mode 22
ReadDataByIdentifier** DIDs. The scaling GM uses varies by ECM and model year,
so every value here is decoded but **must be checked against a known reading on
the car** (the DIAG probe line surfaces the raw byte and, for a wrong DID, the
negative-response code) before its scaling is trusted.

Header: engine parameters are requested from the **ECM at 0x7E0** (response
0x7E8); transmission parameters from the **trans controller at 0x7E2** (response
0x7EA) — confirmed for this truck.

## Confirmed on this vehicle

| Parameter | DID | request | formula | notes |
|---|---|---|---|---|
| Trans fluid (ATF) temp | `0x1940` | `22 19 40` → 7E2 | A − 40 °C | reads plausibly |
| Selector / PRNDL | (broadcast `0x1F5`, not a DID) | — | b3: 1 P / 2 R / 3 N / 4 D | see [sniff-selector.md](sniff-selector.md) |

## Added, awaiting on-car confirmation

| Parameter | DID | request | formula | source |
|---|---|---|---|---|
| Engine oil temp | `0x1154` | `22 11 54` → 7E0 | A − 40 °C | community GM PID list |
| Engine oil pressure | `0x1470` | `22 14 70` → 7E0 | A × (116/256) psi → bar | community GM PID list |

The standard oil-temp PID `0x5C` and the wide diesel PIDs `0x6B` (EGR temp) and
`0x78` (EGT) are **not answered** by this E98, so the enhanced DIDs above are the
real source. Oil pressure decodes to bar and its raw byte is shown on DIAG as
`OILP xx=b.bb` — verify against ~2–3 bar warm idle before adding a gauge card.

## Being probed on the car (standard diesel PIDs)

No open source gives a verified formula for the LWN 2.8 diesel-emissions set, and
the Colorado forums are paywalled. So instead of guessing scaling, the poller
now **probes the standard J1979 diesel PIDs one per cycle** (`obd_poll_tick`
step 6) and the DIAG page shows the **raw bytes of whichever answered**:

| PID | parameter | data bytes | notes |
|---|---|---|---|
| `0x78` | EGT bank 1 | 9 | sensor 1 = (256·B+C)/10−40 °C (already decoded) |
| `0x7A` | DPF differential pressure | 7–9 | scaling TBD from raw |
| `0x7C` | DPF temperature | 9 | (256·B+C)/10−40 °C likely, confirm from raw |
| `0x8B` | diesel aftertreatment status | 7 | bit-encoded regen/soot status |
| `0x86` | particulate matter (soot) | 5 | scaling TBD from raw |
| `0x6B` | EGR temperature | 5 | B−40 °C (this ECM did not answer earlier) |

DIAG line 2 reads `PID xx: b0 b1 b2 …`. **Reading it on the car tells us two
things at once:** which PIDs this ECM actually supports (only supported ones
ever appear), and their raw bytes — from which the real scaling is derived
against known conditions (e.g. DPF dp ≈ 0 at idle, EGT ≈ ambient cold). Then
each gets a proper decode + gauge.

Community hint to check against the raw: on the 2.8 the DPF **soot is reported
in percent, not grams**, and one forum listed soot via `PID 8B` byte C as
`(100/255)·C`.

If a standard PID never answers, the fallback is the SNIFF ANALOG list (a
warm-up / throttle-blip broadcast capture) — the method that found the selector.

## Sources

- [coloradofans — GM PIDs for scantools/OBD tools](https://www.coloradofans.com/threads/gm-pids-for-scantools-obd-tools-torque-app-xgauge-etc.294922/)
- [ssforums — OBD2 PID codes](https://www.ssforums.com/threads/obd2-pid-codes.41346/)
- [Torque forum — GM oil pressure PID](https://torque-bhp.com/community/main-forum/gm-oil-pressure-pid-2/)
- [Harry's LapTimer forum — GM oil temp & pressure, trans temp](http://forum.gps-laptimer.de/viewtopic.php?t=2292)
