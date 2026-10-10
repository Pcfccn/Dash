# Oil pressure: where it comes from, and how to check it

## The source

Oil pressure is read from the **ECM's own sensor value**, GM parameter
**PID `0xA22C`**: one byte, **A × 4 kPa** (= A × 0.04 bar, 0…10.2 bar).

Where that comes from: ScanGauge's X-Gauge list for the **LWN Duramax 2.8**
(Colorado/Canyon, E98 ECM) has *Engine Oil Pressure (PSI)* as
`TXD 07E02CFEA22C  RXF D2E803FE0000  RXD 1808  MTH 001D00320000`, and the Cruze
diesel uses the same entry
([scangauge.com, GM X-Gauge commands](https://www.scangauge.com/support/x-gauge-commands/gm/)).
Decoded:

| Field | Meaning |
|---|---|
| `07E0` | request to the ECM |
| `2C FE A22C` | GMLAN `$2C` DynamicallyDefineMessage: data packet (DPID) `0xFE` = PID `0xA22C` |
| `RXF D2E8 03FE` | answer on `…E8` with byte 0 = `FE`: the UUDT packet on `0x5E8` |
| `RXD 1808` | the value is the 8 bits after the DPID byte |
| `MTH 29/50` | × 0.58 psi = **4 kPa per count** |

The petrol-GM DIDs tried before (`0x115C`, `0x1470`) are a different
parameter set; this E98 refuses them with NRC 31.

## How the firmware asks (`fdcan_obd.c`, "oil pressure")

One step per medium-band slot (about every 21 loop iterations), always one
transaction at a time:

1. **`$22 A22C`** first, a plain read like the other GM DIDs (`0x1154` oil temp
   works this way). Answer `62 A2 2C A` → value.
2. If the ECM **refuses** it (any NRC except busy `21` / conditions `22`), or
   ignores it three times: **`$2C FE A2 2C`** defines packet `0xFE` → answer
   `6C FE`.
3. Then **`$AA 01 FE`** (send the packet once) every slot → a UUDT frame on
   **`0x5E8`**: `FE A …` (no ISO-TP, no SID) → value. A missing or refused
   packet (ECM reset) goes back to step 2.
4. If `$2C` is refused too, the search ends: **`NONE`** on DIAG, the slot goes
   back to fast polling, and OIL P stays `--`. That calibration then has no such
   parameter (e.g. an engine with only an oil-pressure *switch*).

`$2C` is the only service here that is not a pure read. It tells the ECM which
parameter to put into a diagnostic packet; the definition lives in ECM RAM and
is gone at its next reset. No calibration, memory or actuator is touched, and
nothing is cleared — GM scan tools do the same for every live-data screen. A
`0x5E8` packet is decoded only while our own `$AA` read is open, because another
tester on the port may define the same DPID number with something else.

## What the old candidates were

- `0x1BA` byte 3: does not track RPM (0.2 bar at 1599 rpm, 2026-08-23). Not it.
- `0x0C9` byte 2: `0x0C9` is GM's **ECMEngineStatus**, bytes 1–2 = RPM × 4
  (opendbc `gm_global_a_powertrain`; `0x1F5` = ECMPRDNL2 from the same file is
  exactly the selector frame we found, so this bus uses the GM Global A layout).
  On 2026-07-24 at idle `0C9.1` was `0E/0F` and `0C9.2` swept `27..FF`:
  `(0x0F27)/4 ≈ 970 rpm`. Byte 2 was the **RPM low byte**.

Both are gone from the firmware (filters and DIAG fields); filter 2 now takes
`0x5E8`.

## DIAG line 2

```
OILP AA  22/31 2C/-- AA/--  RAW 4B
```

- `OILP 22 | 2C | AA | NONE` — the current step: reading with `$22`, about to
  define with `$2C`, reading the packet with `$AA`, or no source.
- `22/31 2C/-- AA/--` — last NRC per service (`--` = none seen). `22/31` with a
  working `AA` is the expected picture if `$22` is refused.
- `RAW 4B` — last raw byte (hex). `4B` = 75 × 4 kPa = 3.0 bar.

The **OIL P** tile on DRIVE shows the value in bar. Its colour is only judged
with the engine running (RPM ≥ 400); key on / engine off shows the reading in
blue, not as a red alarm.

## Check on the car

1. Key on, engine off, DIAG: within a few seconds `RAW` should show a byte near
   `00` (on path `22` or `AA`), or the path ends at `NONE`. Note what it says.
2. Start, warm up, DIAG + DRIVE photos at:
   - **warm idle** (~800 rpm) — expected roughly 0.8–2 bar;
   - **~2500 rpm held** — should be clearly higher (3–5 bar);
   - **1 s after releasing** — falls back with RPM.
3. Photo of DIAG if the line says `NONE`, with the three NRCs.

Pass: zero with the engine off, rises with RPM, plausible warm idle. Then tune
the OIL P low thresholds in `cluster_config.h` (now 0.8 warn / 0.4 crit, a
guess) against the real warm-idle value.
