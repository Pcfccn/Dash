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

- **Enhanced metrics are not live yet.** ATF temp, DPF soot %, DPF ΔP, distance
  since regen, EGR temp and current gear are enhanced (mode 22) DIDs still set to
  `0x0000 "TBD"` in `cluster_config.h`. They read nothing until you fill the real
  DIDs from an **E98/TCM definition (EFILive / HP Tuners)** for this RG
  calibration. Everything on standard J1979 (speed, RPM, coolant, modelled oil,
  IAT, load, MAP/boost, rail, EGT sensor, battery) works as-is.
- **Main-display backlight polarity.** `BACKLIGHT_DUTY_PCT` in `app_main.c` is
  `0` for hardware diagnosis. With the Si4599 **P-channel** (high-side) MOSFET the
  gate is **active-low**, so 0 % duty holds PA8 LOW = backlight **ON**. Confirm on
  the bench that the 4" backlight is actually at full brightness before the car;
  the PWM duty sense is inverted vs a low-side N-FET and may need the compare
  polarity flipped for correct dimming. Don't blindly set it to `100` — for this
  P-FET that could turn the backlight *off*.
- **Mounting:** secure the board and modules, insulate exposed pins (no shorts to
  chassis), and keep the LCDs out of direct sun-baked dash heat where possible.
