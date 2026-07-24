# Oil-pressure source test (throttle-blip correlation)

## Why

Oil pressure has **no confirmed source** on this E98. The mode-22 DIDs (`0x115C`,
`0x1470`) return NRC 22/31, so it must be a passive broadcast. Two candidate
bytes have been seen on the SNIFF page:

| Candidate | Frame | Byte | Range seen | Provisional formula |
|-----------|-------|------|-----------|---------------------|
| #1 | `0x1BA` | byte 3 | `0x06..0xF6` | `/100` → bar |
| #2 | `0x0C9` | byte 2 | `0x27..0xFF` | `A/36` → bar (guess) |

The 2026-07-24 run showed candidate #1 (`0x1BA.3`) does **not** rise with RPM
(1.5 bar at 2550 rpm — impossible), so neither is trusted yet. This test decides
which byte (if either) is really oil pressure.

## The principle

Real oil pressure **rises with RPM and decays slowly** (1-2 s lag on the way
down). A byte that is just an RPM-derived echo tracks engine speed **instantly
in both directions**. So: blip the throttle and watch which candidate byte
follows RPM up *and lags on the way down*.

## What the firmware now shows (DIAG page)

Both candidates are captured passively and printed at the bottom of the **DIAG**
page, next to live RPM, so a single photo captures the correlation:

```
OILP 1.5 bar  NRC --
RPM 2550  1BA.3=96  0C9.2=A0
```

- `1BA.3` and `0C9.2` are the **raw hex bytes** of the two candidate frames.
- `RPM` is live (OBD keeps polling on DIAG, unlike SNIFF which pauses it).

## Procedure

1. Start the engine, let it **warm up** (oil pressure behaviour is clearest warm;
   cold idle pressure is high and flat).
2. Switch to the **DIAG** page (short-press to cycle: DRIVE → DIAG → SNIFF).
3. Photograph three states, holding each ~2 s so the numbers settle:
   - **Idle** (steady ~800 rpm)
   - **Blip peak** — rev to ~2500-3000 and photograph at the top
   - **Settling** — photograph 1 s *after* releasing the throttle, while RPM is
     dropping back to idle
4. Repeat the blip 2-3 times so the pattern is unambiguous.

## Reading the photos

For each candidate byte, compare its hex value across idle → peak → settling:

- **Tracks RPM up, lags/decays on the way down** → this is oil pressure. Note the
  raw values at known RPMs so the scaling (counts → bar) can be fitted.
- **Mirrors RPM exactly (instant up *and* down)** → it is an RPM echo, not
  pressure. Reject it.
- **Barely moves** → not pressure (could be a temperature or a status byte).

## After the test

Once the real source is known, in `fdcan_obd.c` / `cluster_config.h`:

- Point the `oil_press` decode at the winning frame/byte with the fitted formula.
- Delete the loser's RX branch **and** its acceptance filter in `obd_init`, then
  drop `StdFiltersNbr` back to 3 in `fdcan.c`. The `0x0C9` filter is only added
  for this test (it is a fast frame and loads the shared RX FIFO).
