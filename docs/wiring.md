# Hardware reference — WeAct MiniSTM32H743

Reference notes distilled from [WeActStudio/MiniSTM32H7xx](https://github.com/WeActStudio/MiniSTM32H7xx) (the vendor's official examples/schematic repo for this core board), cross-checked against this project's own code comments. That repo is **not** vendored into this project — it was cloned to a scratch location only to extract the facts below.

## Core board

WeAct "MiniSTM32H7xx" core board, **STM32H743VIT6** variant: ARM Cortex-M7 @ up to 480MHz, 2048KB flash, 1MB RAM. Size 40.64mm × 66.88mm. Input voltage 3.3–5.5V, onboard DC-DC good for 1A max.

## Onboard peripherals this project uses

### Status LED "E3" (PE3)

Push-pull output, **active-low** (`GPIO_PIN_RESET` = lit). Used in this project as the boot/loop heartbeat LED and repurposed to blink a backlight-level code — see `heartbeat_led_init()`/`heartbeat_led_tick()` in `Core/Src/app_main.c`.

### User button "KEY" (PC13)

Input with **pull-down**, so it reads HIGH while pressed (opposite of many reference designs that use pull-up + active-low). Confirmed identical config in the vendor's own `01-GPIO`/`03-LCD_Test` examples. Used for page-cycle / backlight-mode gestures — see `key_button_init()` in `Core/Src/app_main.c`. The board's other two buttons (NRST, BOOT0) are not usable as general-purpose UI inputs.

### Onboard 0.96" ST7735 status display (SPI4)

Pinout confirmed against the vendor's `03-LCD_Test` example (`Core/Src/spi.c`, `Core/Inc/main.h`):

| Signal | Pin | Notes |
|---|---|---|
| SPI4_SCK | PE12 | AF5, push-pull, high speed |
| SPI4_MOSI | PE14 | AF5 — write-only bus, no MISO routed |
| LCD_CS | PE11 | plain GPIO output |
| LCD_DC (WR_RS) | PE13 | plain GPIO output |

SPI4 config used by the vendor demo: master, 1-line (half-duplex TX-only) direction, 8-bit, mode 0 (CPOL=0/CPHA=0), MSB first — matches this project's `st7735_status.c` exactly (`SPI_DIRECTION_1LINE`, `SPI_POLARITY_LOW`, `SPI_PHASE_1EDGE`).

There is **no hardware reset pin** wired to this display from the core board — reset is done in software (matches this project's own comment in `st7735_status.c`).

**Backlight control (PE10) is this project's own addition**, not part of the vendor's factory demo firmware — the vendor's `03-LCD_Test` example never configures PE10 and treats the panel's backlight as always-on. This project's comment in `st7735_status.c` (SI2301 P-channel MOSFET, active-low gate) reflects this project's own schematic reading, not the official example; treat it as unverified against the vendor repo specifically, though it is independently plausible for a P-FET high-side backlight switch.

Panel geometry per the vendor demo and this project's tuning: native 80×160, GRAM window offset (26, 1), BGR order, display inversion on (IPS-type panel).

## Onboard peripherals this project does *not* use (available if ever needed)

Present on the core board per the vendor schematic/examples, not currently touched by this firmware:

| Peripheral | Bus / pins | Vendor example |
|---|---|---|
| 8MB SPI NOR flash | SPI1 — SCK=PB3, MISO=PB4, MOSI=PD7 | `06-SPIFlash_Test` |
| 8MB QSPI NOR flash | QUADSPI — CLK=PB2, NCS=PB6, IO0=PD11, IO1=PD12, IO2=PE2, IO3=PD13 | `02-ExtMem_Boot` |
| MicroSD (TF card) | SDMMC1 — D0..D3=PC8..PC11 (4-bit), + CLK/CMD | `04-SD_Test` |
| 8-bit DVP camera port | DCMI | `05-DCMI_UVC`, `08-DCMI2LCD` |

None of these pins conflict with anything this project uses (SPI2 for the main display, SPI4 for the status display, FDCAN1 for the OBD bus, TIM1 for backlight PWM, PC13/PE3 for KEY/LED).

## What this project adds beyond the core board

Everything else in this firmware is **this project's own wiring**, added on top of the WeAct core board's exposed GPIO headers, not documented in the vendor repo:

- **Main 4" ILI9488 dashboard** (320×480) on **SPI2** — `lv_port_disp.c`. CS=PC0, DC=PC4, RST=PC5 (see that file).
- **FDCAN1** wired to an external CAN transceiver (SN65HVD230) for the vehicle's HS-CAN bus at 500kbps — see `cluster_config.h`. There is no CAN transceiver on the core board itself.
- **Backlight PWM** for the main display on **TIM1_CH1 / PA8**, driving an external MOSFET module gate — see `AppMain_Init()`/`backlight_set()` in `app_main.c`.

## External modules (as purchased)

The three add-on modules this project wires to the core board, identified from the actual AliExpress order.

### Main 4" display module

Generic red **"4.0'' TFT SPI 480X320 V1.0"** module (silkscreen confirmed on the board). AliExpress listing "2.4/2.8/3.2/3.5/4.0 inch SPI TFT LCD ... ST7789 ILI9488 480×320 240×320"; the 4.0" size is the **ILI9488** variant. The **no-touch** variant is fitted (this project uses no touch), so the touch-controller pins on touch versions are absent.

- **Driver:** ILI9488 · **Resolution:** 480×320 (used as 320×480 portrait) · **Interface:** 4-wire SPI · **Logic:** 3.3V (module has onboard regulator/level parts; listing also cites 5V tolerance).
- **Display header signals** (label set on the pin row; match exact order to the board silkscreen, and see `lv_port_disp.c` for the MCU pins actually used): `VCC, GND, CS, RESET, DC/RS, SDI(MOSI), SCK, LED, SDO(MISO)`.
- **Onboard microSD slot** with its **own** SPI pins broken out on the board edge: `SD_CS, SD_MOSI, SD_MISO, SD_SCK`. **Not used by this project** — available if SD logging or a flash-backed config store is ever wanted.

### Backlight control module

**Si4599** dual **N- and P-channel 40V** MOSFET module (expansion board). Driven from **TIM1_CH1 / PA8** PWM — this is the "MOSFET module" referenced by `backlight_set()` in `app_main.c`.

> **Why a P-channel MOSFET (not the IRF520 originally tried):** an **IRF520 is N-channel**, i.e. a **low-side** switch — it makes/breaks the *ground* side of its load. This backlight has **no separate return**: the backlight's minus is tied to the **common ground**, so there is nothing to interrupt on the low side. The switch therefore has to be **high-side**, on the *plus* rail, which needs a **P-channel** MOSFET — hence the Si4599 (its P-channel device) replaced the IRF520. The IRF520 was bought first and did not work for this reason.
>
> **Polarity implication:** a high-side P-FET turns **ON when its gate is pulled LOW**. So PWM/duty sense is inverted vs. a low-side N-FET — PA8 LOW ≈ backlight ON. `app_main.c` compensates with `BACKLIGHT_ACTIVE_LOW 1`, so `backlight_set(100)` = full brightness = PA8 held LOW. Still unverified: whether a 3.3 V gate swing fully turns the FET off with its source on 5 V (Vgs ≈ −1.7 V when "off"); measure Vgs at 0/50/100 %, and if it does not close, add an NPN/N-FET gate driver and set `BACKLIGHT_ACTIVE_LOW 0`.

### CAN transceiver module

**SN65HVD230 (a.k.a. VP230)** CAN board — 3.3V transceiver (listing: DC 3.0–3.6V), matches the chip named in `cluster_config.h`. Bridges FDCAN1 TX/RX to the vehicle's HS-CAN differential pair at 500 kbps. The core board has no onboard CAN transceiver, so this external module is required.

## Boot / programming

- BOOT0 and NRST are physical buttons on the board (separate from the KEY/PC13 user button).
- ISP entry: hold BOOT0 + reset, release reset, then release BOOT0 after ~0.5s (or hold BOOT0 through power-on). DFU over USB-C, or serial via PA9/PA10, using STM32CubeProg.
- This project's own debug/flash path is ST-LINK via STM32CubeIDE (see `Dash.launch`), not the vendor's ISP/DFU flow — the above is a fallback if ST-LINK isn't available.
