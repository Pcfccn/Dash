# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Firmware for a custom instrument cluster replacing the OEM dash in a Holden Colorado RG 2.8 Duramax (E98 ECM). Runs on an STM32H743VITx (WeAct MiniSTM32H743 board), reads live engine/transmission data **read-only** over the vehicle's HS-CAN bus via OBD-II, and renders it with LVGL across two independent SPI displays:

- **Main 4" ILI9488** (320x480, SPI2) — the actual 3-page cluster (DRIVE / DPF / DIAG), driven by `lv_port_disp.c`.
- **Secondary 0.96" ST7735** (160x80, SPI4) — an onboard debug/status screen (`st7735_status.c`), fully independent of the main dashboard's CubeMX config; its SPI/GPIO setup is hand-written directly via HAL rather than through the `.ioc`, specifically so it never touches the main project's peripheral config.

This is a STM32CubeIDE-managed project (Eclipse `.project`/`.cproject`), not a CMake or plain-Makefile project.

## Git workflow: commit and push automatically

After completing a logical change (a feature, fix, or other coherent unit of work — not after every single line edit), commit it with a descriptive message and push to `origin main` immediately, without asking for confirmation first. This repo has no CI and no compile check reachable from a coding-agent shell (no `arm-none-eabi-gcc`/`make` on PATH — the build runs through STM32CubeIDE), so there is no automated gate to wait on; real validation only happens when the user flashes hardware. Push directly to `main` — this is a solo repo, no PR workflow.

Still surface anything a reasonable collaborator would flag before pushing (e.g. a diagnostic/placeholder value left in place, like `BACKLIGHT_DUTY_PCT` being temporarily 0 for hardware debugging) in the commit message or a short note, so it's visible in the history — but do not block the push on it.

## Build / flash / debug

There is no CLI build script — build via **STM32CubeIDE** (the `.project`/`.cproject`/`.mxproject` files are CubeIDE-managed). If building from the command line, use the CubeIDE-generated makefile in `Debug/` (or `Release/` after configuring it) with the `arm-none-eabi-gcc` toolchain that CubeIDE installs:

```
cd Debug && make -j
```

Peripheral/pin config lives in `Dash.ioc` — regenerating code from it via CubeMX/CubeIDE will rewrite the `USER CODE BEGIN/END` guarded sections in `Core/Src/*.c` and `Core/Inc/*.h`; only edit within those guards in CubeMX-owned files (`main.c`, `freertos.c`, `gpio.c`, `spi.c`, `tim.c`, `fdcan.c`, `stm32h7xx_*`) or edits will be lost on regeneration.

Flash/debug config is in `Dash.launch` (ST-LINK GDB server, configured in CubeIDE).

No test framework is present — this is bare-metal firmware with no host-side unit tests.

## Architecture

**Entry point / task flow:** `freertos.c`'s `StartDefaultTask` calls `AppMain_Init()` once, then loops `AppMain_Run()` + `osDelay(5)` forever. Almost all application logic is reached from `Core/Src/app_main.c`, not from `main.c` (which is CubeMX-generated boilerplate + HAL/clock init only).

**`AppMain_Init()`** (`app_main.c`) brings up, in order: the heartbeat LED (PE3), the KEY button (PC13), the main display (`lv_port_disp_init`), the cluster UI + OBD poller (`cluster_app_init`), and the secondary status display (`st7735_status_init`) — then starts the backlight PWM.

**`AppMain_Run()`** (`app_main.c`) is the superloop body: services the heartbeat/backlight-indicator LED, runs the KEY button gesture state machine (short press = next page or +10% backlight depending on mode; 3s hold = toggle page/backlight mode), pumps `lv_timer_handler()`, calls `cluster_app_run()`, and refreshes the status screen every 250ms.

**`cluster_app.c`** is the glue layer between the OBD data source and the UI — it owns no display/panel init itself (that's `lv_port_disp.c`'s job) and just wires `cluster_ui_build()` + `obd_init()` together, then on each `cluster_app_run()` tick: drains CAN RX, runs the OBD-II poll scheduler (~40Hz) and a 1Hz bus watchdog, and refreshes the UI (~25Hz). Set `OBD_DEMO` in `cluster_config.h` to 1 to substitute synthetic values for real CAN traffic (bench testing without a car) — **must be 0 before use in the vehicle**.

**`cluster_config.h`** is the single source of truth for the vehicle-specific domain model: metric keys (`M_SPEED`, `M_RPM`, `M_COOL`, ...), their gauge thresholds/colors (mirrors the `METRICS` table in `design/Diesel_Cluster_v2_3.html`, the HTML mockup this UI was ported from), and the OBD-II PID/DID map (`pid_map[]`) describing how each metric is polled from the ECM (0x7E0) or TCM (0x7E1). Several enhanced-mode (UDS 0x22) DIDs are placeholder `0x0000` entries marked "TBD" — they need real DIDs from an E98/TCM calibration definition (EFILive/HP Tuners) before those metrics (ATF, soot, DPF ∆P, since-regen, EGR temp, gear) will read correctly.

**`fdcan_obd.c`/`.h`** implements the actual OBD-II protocol over FDCAN1 (H743 uses FDCAN, not bxCAN, but this project speaks classic CAN 2.0 frames at 500kbps, not CAN-FD). RX is **polled** from `obd_rx_poll()` in the main loop rather than interrupt-driven — there's no FDCAN NVIC handler wired. Live decoded values land in the global `g_obd` (`obd_data_t`), which both displays read from.

**`cluster_ui.c`/`.h`** builds and refreshes the 3-page LVGL UI (`cluster_ui_build()` once, `cluster_ui_refresh()` at ~25Hz) plus an alert strip; pages are DRIVE(0)/DPF(1)/DIAG(2), selected via `cluster_ui_set_page()`/`cluster_ui_next_page()`.

**Dead code:** `can_gauges.c`/`.h` and `ui_dashboard.c`/`.h` are retired stubs (empty translation units), kept only so the CubeIDE build file list stays valid after the functionality was replaced by `fdcan_obd.c` + `cluster_ui.c`. They're safe to delete along with removing them from the CubeIDE project.

**Design reference:** `design/Diesel_Cluster_v2_3.html` is the standalone HTML/JS mockup of the cluster UI (colors, layout, thresholds) that `cluster_ui.c` and `cluster_config.h` were ported from — check it when the visual design intent of a page/metric is unclear from the C code alone. `design/README.md` is currently empty.

## Use graphify before and after non-trivial changes

This project has a graphify knowledge graph in `graphify-out/` (code call graph + community structure). Before making a non-trivial change — anything touching a function/struct used from more than one file, or where the blast radius isn't obvious from a quick grep — run `/graphify query "<question>"` first to see callers, callees, and cross-community connections instead of guessing from a single-file read. After the change, run `/graphify . --update` so the graph stays current for the next session (cheap: only re-extracts changed files, doesn't re-scan the whole project).

Skip this for trivial edits (renaming a constant, fixing a typo, adjusting a threshold value in `cluster_config.h`) where there's no relationship to trace.

## Hardware notes worth knowing before touching related code

- KEY button (PC13) is pulled down and reads **HIGH when pressed** (opposite of many reference designs) — see `key_button_init()` in `app_main.c`.
- The onboard heartbeat LED (PE3, active-low) doubles as a backlight-level blink-code indicator once past init — see the comment block above `heartbeat_led_tick()` for the encoding, and above `AppMain_Init()`/`heartbeat_led_init()` for what "off / solid / blinking" mean during boot diagnosis.
- The two displays are fully independent SPI buses/GPIO groups (SPI2 for the main ILI9488 via CubeMX, SPI4 for the ST7735 status screen via hand-written HAL init) and must stay that way — `st7735_status.c`'s header comment explains why it intentionally avoids the `.ioc`.
- ILI9488 has no 16bpp SPI mode; `lv_port_disp.c` expands LVGL's RGB565 buffer to 3-byte RGB666 per row (`row_scratch`) on every flush.
