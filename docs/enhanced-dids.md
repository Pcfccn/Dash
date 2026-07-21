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

## Still unmapped (candidates to try)

The diesel emissions set — DPF soot mass, DPF differential pressure, distance
since regen, EGT — did not surface a reliable DID for the **LWN 2.8** (most
community lists are for the 6.6 L LML/LMM). Options, in order of effort:

1. Read them from the broadcast bus via the SNIFF page's ANALOG list (a warm-up
   / throttle-blip capture) — same method that found the selector.
2. Probe candidate mode-22 DIDs and watch the DIAG NRC line: a `0x62` response
   means the DID exists, a `0x7F` NRC means it was rejected. The poller already
   rotates a small probe slot (`obd_poll_tick`, step 6) for exactly this.

## Sources

- [coloradofans — GM PIDs for scantools/OBD tools](https://www.coloradofans.com/threads/gm-pids-for-scantools-obd-tools-torque-app-xgauge-etc.294922/)
- [ssforums — OBD2 PID codes](https://www.ssforums.com/threads/obd2-pid-codes.41346/)
- [Torque forum — GM oil pressure PID](https://torque-bhp.com/community/main-forum/gm-oil-pressure-pid-2/)
- [Harry's LapTimer forum — GM oil temp & pressure, trans temp](http://forum.gps-laptimer.de/viewtopic.php?t=2292)
