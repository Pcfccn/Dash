# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Firmware for a custom auxiliary instrument display in a Holden Colorado RG 2.8 Duramax (E98 ECM). It is an add-on, not a replacement: the OEM cluster stays installed and keeps every legally required tell-tale (ABS, SRS, brakes, indicators, high beam, fuel), so this display does not need to replicate them. Runs on an STM32H743VITx (WeAct MiniSTM32H743 board), reads live engine/transmission data **read-only** over the vehicle's HS-CAN bus via OBD-II, and renders it with LVGL across two independent SPI displays:

- **Main 4" ILI9488** (320x480, SPI2) — the 3-page cluster (DRIVE / DIAG / SNIFF), driven by `lv_port_disp.c`.
- **Secondary 0.96" ST7735** (160x80, SPI4) — an onboard debug/status screen (`st7735_status.c`), fully independent of the main dashboard's CubeMX config; its SPI/GPIO setup is hand-written directly via HAL rather than through the `.ioc`, specifically so it never touches the main project's peripheral config.

This is a STM32CubeIDE-managed project (Eclipse `.project`/`.cproject`), not a CMake or plain-Makefile project.

## Git workflow: commit and push automatically

After completing a logical change (a feature, fix, or other coherent unit of work — not after every single line edit), commit it with a descriptive message and push to `origin main` immediately, without asking for confirmation first. There's no CI, but a **CLI compile check is now available** via STM32CubeCLT (see Build section) — when you've touched compiled code, build it before pushing and only push if it links clean. Behavioural/visual correctness still only shows up when the user flashes hardware. Push directly to `main` — this is a solo repo, no PR workflow.

Still surface anything a reasonable collaborator would flag before pushing (e.g. a diagnostic/placeholder value left in place, like `BACKLIGHT_DUTY_PCT` being temporarily 0 for hardware debugging) in the commit message or a short note, so it's visible in the history — but do not block the push on it.

## Build / flash / debug

Build in the IDE via **STM32CubeIDE**, or from the command line using the CubeIDE-generated makefile in `Debug/` with the **STM32CubeCLT** toolchain (installed at `C:\ST\STM32CubeCLT_1.22.0`). VS Code is wired up in `.vscode/` (build/flash tasks, ST-LINK debug via Cortex-Debug, IntelliSense). CLI build:

```
export PATH="/c/ST/STM32CubeCLT_1.22.0/GNU-tools-for-STM32/bin:/c/ST/STM32CubeCLT_1.22.0/Make/bin:$PATH"
cd Debug && make all -j8
```

Without CubeIDE (fresh clone, no `Debug/`), `python tools/build.py` builds the same Debug configuration with a plain Arm GNU Toolchain 13.3 (found via `--toolchain`, `$ARM_GCC_DIR`, PATH or `~/tools/arm-gnu-toolchain-*/bin`) into `build/`. It compiles from `build/` with `../` paths exactly like CubeIDE, so image sizes are comparable (baseline `f9635ab`: text 593960 vs CubeIDE 593728 — ST's vs Arm's GCC build). It picks up new `Core/Src` files automatically. Its flags are copied from `.cproject`: keep them in sync if the CubeIDE settings change.

Two gotchas with the generated makefile:
- **Use `make all`, never a bare `make`.** The `Debug/makefile` `-include`s the per-folder `subdir.mk` files (which define `clean-*` targets) before its own `all`, so the default goal resolves to *clean* — a bare `make` wipes every `.o`.
- **A new source file added to `Core/Src/` is invisible to the raw CLI build** until it's added to `Debug/Core/Src/subdir.mk` (the C_SRCS/OBJS/C_DEPS lists) **and** `Debug/objects.list` — or you let CubeIDE regenerate them (it auto-discovers new files on Build/Refresh). `Debug/` is git-ignored and CubeIDE-owned; hand-editing it is only for a one-off CLI build.

Peripheral/pin config lives in `Dash.ioc` — regenerating code from it via CubeMX/CubeIDE will rewrite the `USER CODE BEGIN/END` guarded sections in `Core/Src/*.c` and `Core/Inc/*.h`; only edit within those guards in CubeMX-owned files (`main.c`, `freertos.c`, `gpio.c`, `spi.c`, `tim.c`, `fdcan.c`, `stm32h7xx_*`) or edits will be lost on regeneration.

Flash/debug config is in `Dash.launch` (ST-LINK GDB server, configured in CubeIDE).

Host-side unit tests live in `tests/host/`: they `#include` the real `Core/Src/fdcan_obd.c` against a mocked HAL (`tests/host/mock/`) and cover frame validation, ISO-TP, reply ownership, TX errors and MIL freshness. Run them with `python tools/host_test.py` (gcc/clang, or `pip install ziglang`); UBSan is always on, ASan off Windows. When you change `fdcan_obd.c`, run them and add a case for the change. Nothing else (UI, LVGL, drivers) has tests.

## Architecture

**Entry point / task flow:** `freertos.c`'s `StartDefaultTask` calls `AppMain_Init()` once, then loops `AppMain_Run()` + `osDelay(5)` forever. Almost all application logic is reached from `Core/Src/app_main.c`, not from `main.c` (which is CubeMX-generated boilerplate + HAL/clock init only).

**`AppMain_Init()`** (`app_main.c`) brings up, in order: the heartbeat LED (PE3), the KEY button (PC13), the main display (`lv_port_disp_init`), the cluster UI + OBD poller (`cluster_app_init`), and the secondary status display (`st7735_status_init`) — then starts the backlight PWM and, last, switches the IWDG to its run timeout.

**`fault.c`/`.h`**: `main()` (USER CODE Init, right after `HAL_Init`, before `SystemClock_Config`) latches the reset cause (`fault_init`) and starts IWDG1 with a ~10 s boot timeout; `AppMain_Init` switches it to ~3 s, fed only at the end of a complete `AppMain_Run`. HardFault, FreeRTOS stack overflow (`configCHECK_FOR_STACK_OVERFLOW 2`) and `Error_Handler` record a code in the `.noinit` section (DTCM, see the linker script) and stop, so the IWDG resets the board instead of freezing the last frame on the panel; DIAG shows `LAST RESET: ...` on the next boot. `FAULT_TEST` in `fault.h` (default 0, must be 0 in the car) triggers a hang or a HardFault for a bench check. Anything that legitimately blocks the loop for more than ~3 s will now reset the board — keep that in mind before adding long blocking calls.

**`AppMain_Run()`** (`app_main.c`) is the superloop body: services the heartbeat/backlight-indicator LED, runs the KEY button gesture state machine (short press = next page or +10% backlight depending on mode; 3s hold = toggle page/backlight mode), pumps `lv_timer_handler()`, calls `cluster_app_run()`, and refreshes the status screen every 250ms.

**`cluster_app.c`** is the glue layer between the OBD data source and the UI — it owns no display/panel init itself (that's `lv_port_disp.c`'s job) and just wires `cluster_ui_build()` + `obd_init()` together, then on each `cluster_app_run()` tick: drains CAN RX, runs the OBD-II poll scheduler (~40Hz) and a 1Hz bus watchdog, and refreshes the UI (~25Hz). Set `OBD_DEMO` in `cluster_config.h` to 1 to substitute synthetic values for real CAN traffic (bench testing without a car) — **must be 0 before use in the vehicle**.

**`cluster_config.h`** is the single source of truth for the vehicle-specific domain model: metric keys (`M_SPEED`, `M_RPM`, `M_COOL`, ...), their gauge thresholds/colors (mirrors the `METRICS` table in `design/Diesel_Cluster_v2_3.html`, the HTML mockup this UI was ported from), and the OBD-II PID/DID map (`pid_map[]`) describing how each metric is polled from the ECM (0x7E0) or TCM (0x7E1). Several enhanced-mode (UDS 0x22) DIDs are placeholder `0x0000` entries marked "TBD" — they need real DIDs from an E98/TCM calibration definition (EFILive/HP Tuners) before those metrics will read correctly.

What this truck actually answers, verified in-vehicle (don't re-derive it by guessing):

| Signal | Source | Status |
|---|---|---|
| coolant `0x05`, IAT `0x0F`, speed `0x0D`, RPM `0x0C`, load `0x04`, MAP `0x0B`, baro `0x33`, rail `0x23`, batt `0x42` | mode 01 | works |
| ATF temp | DID `0x1940` on 0x7E2 | works, `A - 40` |
| gear | DID `0x199A` on 0x7E2 | works, but it is the **engaged gear ratio, not the selector range** — D1/D2/D3 give 1/2/3 and P also gives 1, so it cannot express P/R/N |
| oil temp `0x5C`, EGR temp `0x6B` | mode 01 | **not supported** — silently omitted from grouped replies rather than refused |
| selector range (PRNDL) | broadcast `0x1F5` byte 3 | works: 1 P / 2 R / 3 N / 4 D (found with the SNIFF page) |
| oil pressure | — | **no confirmed source**: mode-22 DIDs refused; broadcast candidates `0x1BA[3]` (rejected) and `0x0C9[2]` (unverified) are shown raw on DIAG only |
| soot, DPF ∆P, since-regen | — | **not available** on this ECM; the DPF page was removed |

Note a grouped mode-01 request drops unsupported PIDs from the reply instead of returning a negative response, so a missing value there is not an error and produces no NRC.

**`fdcan_obd.c`/`.h`** implements the actual OBD-II protocol over FDCAN1 (H743 uses FDCAN, not bxCAN, but this project speaks classic CAN 2.0 frames at 500kbps, not CAN-FD). RX is **polled** from `obd_rx_poll()` in the main loop rather than interrupt-driven — there's no FDCAN NVIC handler wired. Live decoded values land in the global `g_obd` (`obd_data_t`), which both displays read from.

These conventions in the OBD path matter and are easy to break:

- **One diagnostic transaction at a time.** `request()` refuses while one is open and `obd_poll_tick()` only ages it. It closes on a valid answer, an NRC for its service (NRC 0x78 extends it), a refused Flow Control, a rejected FF/CF, or its tick timeout; a multi-frame answer belongs to it and closes only it. Don't add a send path that bypasses `request()`.
- **Every received byte is length-checked before it is decoded.** `obd_rx_poll()` checks the DLC for each broadcast, ISO-TP SF/FF/CF lengths, and validates a whole mode-01 reply (known PIDs, exact fit) before writing anything; mode 01 is only accepted from the ECM and each mode-22 DID only from the module it is requested from. Rejected frames increment the `BAD` counter on DIAG. Never decode a missing byte as 0.
- **Every float starts as `NaN`, meaning "the bus has never sent this."** The UI renders NaN as `--`. Do not "fix" a metric by defaulting it to 0 — an unsupported PID would then paint a believable lie (0 V battery, 0 °C oil), which is exactly the bug that once made an entirely dead poller look like a working one.
- **Any timeout reached from the superloop must be counted in poll ticks, not `HAL_GetTick()` milliseconds.** Rendering and the display flush still hold the loop: the flush now goes out by DMA, but LVGL renders on the CPU and each flush first waits for the previous area's DMA, so a full repaint still costs ~200 ms of wire time at 18.75 MHz (it used to be a blocking row-by-row `HAL_SPI_Transmit` at 9.4 MHz, ~400 ms). DIAG line 1 shows `LOOP`, the longest loop period over the last second. A wall-clock deadline expires before `obd_rx_poll()` next runs; that silently killed every multi-frame ISO-TP reply while single-frame ones kept working. This is a workaround for RX being polled from the same loop as the blocking display flush, not a goal: once FDCAN RX is interrupt-driven into a queue (planned stage C), real millisecond deadlines become correct and preferable. Staleness the user sees (MIL/DTC) is likewise counted in unanswered requests, not ms, for the same reason.

**`cluster_ui.c`/`.h`** builds and refreshes the 3-page LVGL UI (`cluster_ui_build()` once, `cluster_ui_refresh()` at ~25Hz) plus an alert strip; pages are DRIVE(0)/DIAG(1)/SNIFF(2), selected via `cluster_ui_set_page()`/`cluster_ui_next_page()`.

**`can_sniff.c`/`.h`** is a workbench-only passive CAN change detector behind the SNIFF page: it widens the FDCAN acceptance filter to all standard IDs and lists the bytes that changed most recently, so operating one control identifies the frame carrying it. It exists because the signals still missing an identifier (selector range/PRNDL, soot, DPF ∆P, since-regen) are broadcast as ordinary frames — the OEM cluster displayed them. Listening is deliberately tied to the page being visible: the wide filter competes with OBD replies for the same RX FIFO, and `obd_poll_tick()` is suspended while it runs.

**Dead code:** `can_gauges.c`/`.h` and `ui_dashboard.c`/`.h` are retired stubs (empty translation units), kept only so the CubeIDE build file list stays valid after the functionality was replaced by `fdcan_obd.c` + `cluster_ui.c`. They're safe to delete along with removing them from the CubeIDE project.

**Design reference:** `design/Diesel_Cluster_v2_3.html` is the standalone HTML/JS mockup of the cluster UI (colors, layout, thresholds) that `cluster_ui.c` and `cluster_config.h` were ported from — check it when the visual design intent of a page/metric is unclear from the C code alone. `design/README.md` is currently empty.

## Use graphify before and after non-trivial changes

This project has a graphify knowledge graph in `graphify-out/` (code call graph + community structure). Before making a non-trivial change — anything touching a function/struct used from more than one file, or where the blast radius isn't obvious from a quick grep — run `/graphify query "<question>"` first to see callers, callees, and cross-community connections instead of guessing from a single-file read. After the change, run `/graphify . --update` so the graph stays current for the next session (cheap: only re-extracts changed files, doesn't re-scan the whole project).

Skip this for trivial edits (renaming a constant, fixing a typo, adjusting a threshold value in `cluster_config.h`) where there's no relationship to trace.

## Hardware notes worth knowing before touching related code

- KEY button (PC13) is pulled down and reads **HIGH when pressed** (opposite of many reference designs) — see `key_button_init()` in `app_main.c`. It also raises EXTI15_10 on both edges: the ISR latches presses so a tap that starts and ends inside one (flush-stalled) loop iteration is still counted.
- The onboard heartbeat LED (PE3, active-low) doubles as a backlight-level blink-code indicator once past init — see the comment block above `heartbeat_led_tick()` for the encoding, and above `AppMain_Init()`/`heartbeat_led_init()` for what "off / solid / blinking" mean during boot diagnosis.
- The two displays are fully independent SPI buses/GPIO groups (SPI2 for the main ILI9488 via CubeMX, SPI4 for the ST7735 status screen via hand-written HAL init) and must stay that way — `st7735_status.c`'s header comment explains why it intentionally avoids the `.ioc`.
- ILI9488 has no 16bpp SPI mode; `lv_port_disp.c` expands each flushed area from LVGL's RGB565 into 3-byte RGB666 in `tx_buf`, then sends it by DMA (DMA1 Stream0 -> SPI2 TX) and calls `lv_display_flush_ready()` immediately; the next flush waits for that DMA (bounded at 100 ms). `spi2_dma_init()` in the same file re-inits SPI2 at /8 = 18.75 MHz, raises the PB13-15 pin speed and owns the DMA stream and the `DMA1_Stream0`/`SPI2` IRQ handlers. This is deliberately outside the CubeMX files, so the `.ioc` still says /32 and no DMA; do not enable SPI2 DMA in CubeMX without removing these (the link would fail on duplicate handlers).
- **I-cache and D-cache are on** (`main.c`, USER CODE Init). All `.data`/`.bss`/stack live in AXI SRAM (cacheable); `.noinit` (the fault record) is in DTCM, which is never cached. The display flush cleans its buffer before each DMA. **Any new DMA must clean (TX) or invalidate (RX) its buffers, 32-byte aligned, or use a non-cacheable MPU region**, otherwise it reads or writes stale data. DMA1/DMA2 cannot reach DTCM at all.
- UI refresh (`cluster_ui_refresh()`, 25 Hz) goes through `ui_text()`/`ui_text_color()`/`ui_bg_color()`/`ui_border_color()`, which skip unchanged values: in LVGL 9.3 `lv_label_set_text` and `lv_obj_set_style_*` invalidate (= repaint over SPI) even when nothing changed. Use them for any new widget updated from the refresh path.

## graphify

This project has a knowledge graph at graphify-out/ with god nodes, community structure, and cross-file relationships.

Rules:
- For codebase questions, first run `graphify query "<question>"` when graphify-out/graph.json exists. Use `graphify path "<A>" "<B>"` for relationships and `graphify explain "<concept>"` for focused concepts. These return a scoped subgraph, usually much smaller than GRAPH_REPORT.md or raw grep output.
- If graphify-out/wiki/index.md exists, use it for broad navigation instead of raw source browsing.
- Read graphify-out/GRAPH_REPORT.md only for broad architecture review or when query/path/explain do not surface enough context.
- After modifying code, run `graphify update .` to keep the graph current (AST-only, no API cost).
