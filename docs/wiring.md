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
- **Backlight PWM** for the main display on **TIM1_CH1 / PA8**, driving an IRF520 MOSFET gate — see `AppMain_Init()`/`backlight_set()` in `app_main.c`.

## Boot / programming

- BOOT0 and NRST are physical buttons on the board (separate from the KEY/PC13 user button).
- ISP entry: hold BOOT0 + reset, release reset, then release BOOT0 after ~0.5s (or hold BOOT0 through power-on). DFU over USB-C, or serial via PA9/PA10, using STM32CubeProg.
- This project's own debug/flash path is ST-LINK via STM32CubeIDE (see `Dash.launch`), not the vendor's ISP/DFU flow — the above is a fallback if ST-LINK isn't available.
