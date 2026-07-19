# In-vehicle install — from bench demo to a live car

How to take this cluster off the desk (USB-powered, `OBD_DEMO 1`) and run it in
the Holden Colorado RG for real: powering the WeAct board from the car, tapping
the HS-CAN bus through the SN65HVD230, and the bring-up sequence.

> **Firmware prerequisite:** `OBD_DEMO` in `Core/Inc/cluster_config.h` must be
> `0` (real CAN). It is now 0. With demo on, the screens show synthetic values
> and the CAN transceiver is never actually read.

---

## 1. Powering the board in the car

The WeAct MiniSTM32H743 wants **3.3–5.5 V** on its 5 V/USB input (onboard DC-DC,
~1 A). The car's electrical system is nominally 12 V but swings **~9 V during
cranking to ~14.8 V charging**, with much larger transient spikes (load dump).
You **cannot** feed 12 V straight in, and you **must** protect against the noise.

**Use a 12 V → 5 V step-down (buck) converter:**

- Pick an **automotive-grade buck** with a wide input range (e.g. 8–40 V in) and
  **≥ 2 A at 5 V** output. The board + 4" ILI9488 + its backlight + the 0.96"
  status panel can pull several hundred mA; 2 A gives headroom.
- A quality **12 V USB car-charger module** (the guts of a car phone charger)
  works and already has automotive input filtering — feed its 5 V/USB into the
  board's USB/5 V.
- Add a **1–3 A inline fuse** on the 12 V feed, as close to the tap as possible.
- Add **reverse-polarity protection** (a series Schottky, or a P-FET ideal-diode)
  if your buck doesn't have it.

**Where to tap 12 V — use IGNITION-SWITCHED power, not constant:**

- Tap a **switched/accessory (ACC) circuit** so the cluster powers down with the
  car. A fuse-tap ("add-a-fuse") on an ACC fuse in the cabin fuse box is the
  clean way.
- **Do NOT** power from OBD pin 16 (constant +12 V) for a permanent install — it
  never switches off and will **flatten the battery** over days. (Fine for a quick
  bench/driveway test only.)
- During engine **crank** the rail dips; the board may brown-out/reset on start.
  That's acceptable — it comes back once the alternator is up. If you want it to
  survive cranking, use a buck with a low dropout and a bulk input capacitor.

**Grounding:** one common ground. The buck's ground, the board's ground, and the
SN65HVD230's ground all tie to **vehicle chassis ground** (OBD pin 4/5 or a body
ground bolt). Use a single star point to avoid ground loops.

**Where the 5 V actually connects on the board — there is NO power jack.** Per the
WeAct V1.2 schematic the power path is:

```
USB-C VBUS ─► D10 (Schottky) ─► 5V rail ─► DC-DC U2 (1-2 A) ─► 3V3 rail
```

Feed the buck's output into the board's supply input:

- Buck **+5 V → the `V+` pin** (next to the DC-DC input caps). This is the entry
  of the **5 V rail** that feeds the regulator; it's the same net as the header
  `5V` pins, so those work too, but `V+`/`GND` is the intended input pair.
- Buck **GND → the `GND`** beside it.
- Supply **5 V (max 5.5 V — DC-DC input limit).** The board makes 3.3 V from it and
  the same 5 V rail feeds the panel backlight.
- The `3V3` pins are the regulator **output** — do NOT inject supply there.
  `VBAT` is RTC backup only (BAT54C), not a power input.
- USB is **diode-isolated** from the 5 V rail by D10, so external 5 V can't
  back-feed the USB port — but still cleanest to run one source at a time.
- Multimeter sanity check: `V+` ↔ header `5V` = continuity (same rail); `V+` ↔
  `3V3` = none (post-regulator); `V+` ↔ USB-C 5 V = reads through D10 as a diode
  drop (~0.2-0.3 V), NOT a dead short — that's normal.

**Current display wiring (bench, keep it):** the 4" panel's **VCC is on the
board's 3.3 V** and its **backlight is on the board's 5 V**. Feeding the board 5 V
as above reproduces this 1:1 — the board passes 5 V to the backlight and makes
3.3 V for VCC, just like USB does. (This is also why a 3.3 V-only supply won't
work as wired: the backlight would lose its 5 V.)

---

## 2. Connecting to the CAN bus (OBD-II port)

The RG's **HS-CAN** (ISO 15765-4, 500 kbps — what this firmware speaks) is on the
**OBD-II / DLC connector** under the dash:

| OBD-II pin | Signal | Connect to |
|---|---|---|
| **6** | **CAN-H** | SN65HVD230 **CANH** |
| **14** | **CAN-L** | SN65HVD230 **CANL** |
| 4 | Chassis GND | common ground |
| 5 | Signal GND | common ground |
| 16 | +12 V constant | (bench test power only — see above) |

**SN65HVD230 transceiver (3.3 V):**

- **VCC = 3.3 V** — this is a 3.3 V part; do **not** feed it 5 V. Take 3.3 V from
  the board's 3V3 pin.
- **TXD → PD1** (MCU `FDCAN1_TX`), **RXD → PD0** (MCU `FDCAN1_RX`). 3.3 V logic
  both sides — direct wire, no level shifter.
- **Rs pin → GND** for high-speed mode (fine for 500 kbps). Most breakout modules
  handle this on-board.
- Keep **CANH/CANL as a twisted pair** and the stub from the OBD port short.

### Sharing the OBD port with another device

Running this cluster alongside a scan tool / insurance dongle / logger (Y-splitter
or a second tap on pins 6 + 14) is fine — CAN is multi-drop and this firmware is
read-only. Two rules:

- **Do not add a termination resistor.** The vehicle already has 2 x 120 Ω (60 Ω
  total). Many SN65HVD230 breakout boards ship with a 120 Ω resistor populated —
  remove/disable it, or the bus impedance drops far enough to cause errors.
- **Keep each stub short** (< ~30 cm); long branches reflect at 500 kbps.

Protocol-wise the firmware tolerates a second tester: it only answers a
multi-frame **First Frame** with Flow Control when it is the one that sent the
matching request (`await_resp_id` in `fdcan_obd.c`). Without that gate both
readers would send Flow Control for the same transfer and corrupt each other's
multi-frame reads (mode 22 DIDs, mode 03 DTC lists). Single-frame replies are
decoded regardless of who asked, and `decode_mode01()` keys off the echoed PID —
so the other tool's polling transparently feeds these gauges as a bonus.

### ⚠️ Bus termination — the #1 thing to get right

HS-CAN is already terminated by the vehicle: **120 Ω at each end = 60 Ω total**.
Many SN65HVD230 breakout boards ship with **their own 120 Ω resistor on board**.
If you add that as a third terminator you load the bus to ~40 Ω and can cause
bus errors / no comms.

- **Remove or disable the module's 120 Ω resistor** when tapping the live bus.
- **Verify:** ignition OFF, measure resistance CANH↔CANL at the OBD port — a
  healthy bus reads **~60 Ω**. If you see ~40 Ω, your module's resistor is still
  in circuit — take it out.

---

## 3. Read-only safety

This firmware is **read-only** — it only *requests* live data (OBD mode 01) and
reads DTCs (mode 03/07) and enhanced DIDs (mode 22). It never writes calibrations
and **never clears codes** (mode 04 is intentionally unused). It is still an
**active bus node** (it transmits request frames, like any scan tool), which is
normal and low-risk. Do the first power-up **key-on / engine-off (KOEO)** to
validate before driving.

---

## 4. Bring-up sequence

1. **Flash** the firmware with `OBD_DEMO 0` (done) via ST-LINK.
2. *(Optional but recommended)* Bench-test CAN first with a USB-CAN adapter or an
   OBD-II simulator, so you know the transceiver + wiring work before the car.
3. In the car, **KOEO**: connect CAN + power (buck on switched 12 V).
   - Check the small status screen: **CAN OK** (green), speed `0`, battery
     **~12.4 V**.
   - If CAN shows `--` (red): re-check termination (60 Ω), CANH/CANL not swapped,
     transceiver VCC = 3.3 V, and PD0/PD1 not reversed.
4. **Verify termination** reads ~60 Ω (ignition off) if you haven't already.
5. **Start the engine**: RPM, coolant, load, MAP/boost, rail pressure and battery
   should come alive on the main 4" cluster.
6. **Road-test** safely (ideally a passenger watches the screen, or parked).

---

## 5. Known gaps for a "complete" in-car build

- **Enhanced metrics — partially wired, unverified on the car.** The poller now
  requests, with community-sourced (not yet car-verified) DIDs:
  - **ATF / trans fluid temp** — mode 22 DID `0x1940` on the trans controller
    (`0x7E2` → `0x7EA`), `A − 40 °C`.
  - **Current gear** — mode 22 DID `0x199A` on `0x7E2`, raw index in byte A.
  - **EGR temp** — standard J1979 mode 01 PID `0x6B`, sensor 1 = `B − 40 °C`.

  If any of these stay blank in the car, the module/DID/scaling is wrong for this
  truck — the request is in `obd_poll_tick()` and the decode in `decode_mode22()`
  / `decode_mode01()` (`fdcan_obd.c`); adjust there. **DPF soot %, DPF ΔP and
  distance-since-regen are NOT polled** — GM guards those and the DIDs vary by
  model year, so discover the working DIDs on the actual truck (BiScan for GM /
  Gretio / Torque GM set) and add them. Everything on standard J1979 (speed, RPM,
  coolant, modelled oil, IAT, load, MAP/boost, rail, EGT sensor, battery) works
  as-is.
- **Main-display backlight polarity.** `BACKLIGHT_DUTY_PCT` in `app_main.c` is
  `0` for hardware diagnosis. With the Si4599 **P-channel** (high-side) MOSFET the
  gate is **active-low**, so 0 % duty holds PA8 LOW = backlight **ON**. Confirm on
  the bench that the 4" backlight is actually at full brightness before the car;
  the PWM duty sense is inverted vs a low-side N-FET and may need the compare
  polarity flipped for correct dimming. Don't blindly set it to `100` — for this
  P-FET that could turn the backlight *off*.
- **Mounting:** secure the board and modules, insulate exposed pins (no shorts to
  chassis), and keep the LCDs out of direct sun-baked dash heat where possible.

---

## Appendix: full connection & power map

### Power distribution

Everything hangs off the WeAct board — the buck feeds ONLY the board's `5V`
pin, exactly like USB does now.

```
Car 12 V  (IGNITION/ACC, inline fuse 2-3 A)
  │
  └─►  12 V → 5 V buck (automotive, ≥ 2 A)
             +5V │              │ GND
                 ▼              ▼
        WeAct pin `5V`     WeAct pin `GND`
        (= USB 5V net, no protection — do NOT also plug in USB)
                 │
    ── on the board, already wired ─────────────────────────────
        board passes 5 V ───────────►  4" backlight LED+  (5 V)
        board regulator → 3V3 rail ─►  4" panel VCC        (3.3 V)
                                   └─►  SN65HVD230 VCC      (3.3 V, never 5 V)
                                   └─►  STM32H743 + on-board 0.96" ST7735

GND: one common star ground — buck GND · WeAct GND · display GND · transceiver
     GND · vehicle chassis (OBD pin 4/5).
```

### Signal wiring (all MCU I/O is 3.3 V logic — do not exceed ~3.6 V on any pin)

| MCU pin | Function | Connect to |
|---|---|---|
| PB13 | SPI2 SCK | 4" ILI9488 **SCK** |
| PB15 | SPI2 MOSI | 4" **SDI/MOSI** |
| PB14 | SPI2 MISO | 4" **SDO/MISO** (optional — panel is write-only; may leave off) |
| PC0 | GPIO | 4" **CS** |
| PC4 | GPIO | 4" **DC/RS** |
| PC5 | GPIO | 4" **RESET** |
| PA8 | TIM1_CH1 PWM | **Si4599 gate** (backlight; active-LOW for the P-FET) |
| PD1 | FDCAN1_TX | SN65HVD230 **TXD/D** |
| PD0 | FDCAN1_RX | SN65HVD230 **RXD/R** |
| — | CAN bus | SN65HVD230 **CANH → OBD pin 6**, **CANL → OBD pin 14** (twisted pair) |
| — | transceiver mode | SN65HVD230 **Rs → GND** (high-speed) |

**On-board already, nothing to wire:** 0.96" ST7735 status screen (SPI4: SCK PE12,
MOSI PE14, CS PE11, DC PE13, backlight PE10), KEY button (PC13), heartbeat LED (PE3).

### Voltage / current — how much can/should each take

| Node | Feed it | Draw (typical) | Hard limit |
|---|---|---|---|
| 5 V rail (buck out) | 5 V | ~0.4–0.6 A total | size buck ≥ 2 A |
| WeAct board input | 5 V (3.3–5.5 V ok) | — | onboard DC-DC 3V3 rail = **1 A max** |
| STM32H743 (3V3) | 3.3 V (from board) | ~0.2–0.3 A | — |
| 4" panel VCC (logic) | 3.3 V (from board 3V3) | ~20–40 mA | 3.3–5 V |
| 4" backlight | 5 V (from board 5V pin) | ~40–120 mA | — |
| SN65HVD230 VCC | **3.3 V** (from board) | ~20 mA | **3.6 V max — never 5 V** |
| OBD pin 16 (+12 V) | 12 V constant | bench test only, fuse it | drains battery if left on |

Notes:
- As wired now, **everything is fed through the WeAct board** — you only connect
  the buck's 5 V to the board's `5V` pin. The board passes 5 V to the backlight and
  its regulator makes 3.3 V for the panel VCC, transceiver, MCU and ST7735.
- The backlight's 5 V draw passes through the board's `5V`/USB net (not the 1 A
  3V3 DC-DC), so the onboard regulator only carries the 3.3 V loads.
- Backlight PWM sense is inverted (P-FET): `BACKLIGHT_DUTY_PCT 0` in `app_main.c`
  holds PA8 LOW = backlight ON. Confirm on the bench before wiring in the car.
