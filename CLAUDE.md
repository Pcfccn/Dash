# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Firmware for a custom auxiliary instrument display in a Holden Colorado RG 2.8 Duramax (E98 ECM). It is an add-on, not a replacement: the OEM cluster stays installed and keeps every legally required tell-tale (ABS, SRS, brakes, indicators, high beam, fuel), so this display does not need to replicate them. Runs on an STM32H743VITx (WeAct MiniSTM32H743 board), reads live engine/transmission data **read-only** over the vehicle's HS-CAN bus via OBD-II, and renders it with LVGL across two independent SPI displays:

- **Main 4" ILI9488** (320x480, SPI2) — the 3-page cluster (DRIVE / DIAG / SNIFF), driven by `lv_port_disp.c`.
- **Secondary 0.96" ST7735** (160x80, SPI4) — an onboard debug/status screen (`st7735_status.c`), fully independent of the main dashboard's CubeMX config; its SPI/GPIO setup is hand-written directly via HAL rather than through the `.ioc`, specifically so it never touches the main project's peripheral config.

This is a STM32CubeIDE-managed project (Eclipse `.project`/`.cproject`), not a CMake or plain-Makefile project.

## Git workflow: commit and push automatically

After completing a logical change (a feature, fix, or other coherent unit of work — not after every single line edit), commit it with a descriptive message and push to `origin main` immediately, without asking for confirmation first. There's no CI, but a **CLI compile check is now available** via STM32CubeCLT (see Build section) — when you've touched compiled code, build it before pushing and only push if it links clean. Behavioural/visual correctness still only shows up when the user flashes hardware. Push directly to `main` — this is a solo repo, no PR workflow.

Still surface anything a reasonable collaborator would flag before pushing (e.g. a diagnostic/placeholder value left in place, like `OBD_DEMO` or `FAULT_TEST` left non-zero) in the commit message or a short note, so it's visible in the history — but do not block the push on it.

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

`Dash.ioc` now matches the code for everything that matters (FDCAN 4 std filters + 16-deep RX FIFO0, default task 4096 words = 16 KB, heap 64 KB, SPI2 mode 0 at /16), and the firmware no longer depends on regeneration getting it right: `obd_init()` re-lays out the FDCAN message RAM if the counts are short, `lv_port_disp.c` re-inits SPI2 with every setting the panel needs (1LINE stays code-only — changing it in CubeMX would also drop the MISO pin), a `_Static_assert` in `fault.c` fails the build if the heap shrinks, and a shrunk stack shows up as `LAST RESET: STACK OVERFLOW`. After any regeneration, still read the `git diff` of the generated files before building: expect `spi.c` to go back to 2LINES and lose its comments (harmless, overridden), nothing else functional.

Flash/debug config is in `Dash.launch` (ST-LINK GDB server, configured in CubeIDE).

Host-side unit tests live in `tests/host/`: they `#include` the real `Core/Src/fdcan_obd.c` against a mocked HAL (`tests/host/mock/`) and cover frame validation, ISO-TP, reply ownership, TX errors and MIL freshness. Run them with `python tools/host_test.py` (gcc/clang, or `pip install ziglang`); UBSan is always on, ASan off Windows. When you change `fdcan_obd.c`, run them and add a case for the change. Nothing else (UI, LVGL, drivers) has tests.

## Architecture

**Entry point / task flow:** `freertos.c`'s `StartDefaultTask` calls `AppMain_Init()` once, then loops `AppMain_Run()` + `osDelay(5)` forever. Almost all application logic is reached from `Core/Src/app_main.c`, not from `main.c` (which is CubeMX-generated boilerplate + HAL/clock init only).

**`AppMain_Init()`** (`app_main.c`) brings up, in order: the heartbeat LED (PE3), the KEY button (PC13), the main display (`lv_port_disp_init`), the cluster UI + OBD poller (`cluster_app_init`), and the secondary status display (`st7735_status_init`) — then starts the backlight PWM and, last, switches the IWDG to its run timeout.

**`fault.c`/`.h`**: `main()` (USER CODE Init, right after `HAL_Init`, before `SystemClock_Config`) latches the reset cause (`fault_init`) and starts IWDG1 with a ~10 s boot timeout; `AppMain_Init` switches it to ~3 s, fed only at the end of a complete `AppMain_Run`. HardFault, FreeRTOS stack overflow (`configCHECK_FOR_STACK_OVERFLOW 2`) and `Error_Handler` record a code in the `.noinit` section (DTCM, see the linker script) and stop, so the IWDG resets the board instead of freezing the last frame on the panel; DIAG shows `LAST RESET: ...` on the next boot, with `xN` when N abnormal resets came in a row (the streak lives in the same record and clears after 60 s of clean running), so a reset storm is not mistaken for a one-off. `FAULT_TEST` in `fault.h` (default 0, must be 0 in the car) triggers a hang (1), a HardFault (2) or a run of main-panel SPI errors (3, no reset: exercises the display recovery) for a bench check. Anything that legitimately blocks the loop for more than ~3 s will now reset the board — keep that in mind before adding long blocking calls.

**`AppMain_Run()`** (`app_main.c`) is the superloop body: services the heartbeat/backlight-indicator LED, runs the KEY button gesture state machine (short press = next page or +10% backlight depending on mode; 3s hold = toggle page/backlight mode), pumps `lv_timer_handler()`, calls `cluster_app_run()`, and refreshes the status screen every 250ms.

**`cluster_app.c`** is the glue layer between the OBD data source and the UI — it owns no display/panel init itself (that's `lv_port_disp.c`'s job) and just wires `cluster_ui_build()` + `obd_init()` together, then on each `cluster_app_run()` tick: drains CAN RX, runs the OBD-II poll scheduler (~40Hz) and a 1Hz bus watchdog, and refreshes the UI (~25Hz). Set `OBD_DEMO` in `cluster_config.h` to 1 to substitute synthetic values for real CAN traffic (bench testing without a car) — **must be 0 before use in the vehicle**.

**`cluster_config.h`** is the single source of truth for the vehicle-specific domain model: metric keys (`M_SPEED`, `M_RPM`, `M_COOL`, ...), their gauge thresholds/colors (mirrors the `METRICS` table in `design/Diesel_Cluster_v2_3.html`, the HTML mockup this UI was ported from), per-metric freshness limits (`metric_stale_ms`), and the CAN IDs / OBD addresses. How each metric is requested lives in `obd_poll_tick()` and its decoder in `fdcan_obd.c` (the old documentation-only `pid_map[]` was removed after it drifted). The gauge thresholds come from the HTML mockup, not from an engine calibration.

What this truck actually answers, verified in-vehicle (don't re-derive it by guessing):

| Signal | Source | Status |
|---|---|---|
| coolant `0x05`, IAT `0x0F`, speed `0x0D`, RPM `0x0C`, load `0x04`, MAP `0x0B`, baro `0x33`, rail `0x23`, batt `0x42` | mode 01 | works |
| ATF temp | DID `0x1940` on 0x7E2 | works, `A - 40` |
| gear | DID `0x199A` on 0x7E2 | works, but it is the **engaged gear ratio, not the selector range** — D1/D2/D3 give 1/2/3 and P also gives 1, so it cannot express P/R/N |
| oil temp `0x5C`, EGR temp `0x6B` | mode 01 | **not supported** — silently omitted from grouped replies rather than refused |
| selector range (PRNDL) | broadcast `0x1F5` byte 3 | works: 1 P / 2 R / 3 N / 4 D (found with the SNIFF page) |
| oil pressure | ECM PID `0xA22C`, `A × 4 kPa` (ScanGauge, LWN 2.8) | **wired, not yet seen in-vehicle**: `$22 A22C` first, else GMLAN `$2C FE A22C` + `$AA 01 FE` → UUDT `0x5E8` `[FE][A]`; path/NRCs/raw on DIAG line 2 (docs/oil-pressure-test.md). Shown neutral, never a warning colour, until `OILP_VALIDATED 1`. The petrol-GM DIDs `0x115C`/`0x1470` are refused (NRC 31). Old broadcast candidates are dead: `0x1BA[3]` does not track RPM, `0x0C9[2]` is the RPM low byte (`0x0C9` = ECMEngineStatus, bytes 1–2 = RPM×4) |
| soot, DPF ∆P, since-regen | — | **not available** on this ECM; the DPF page was removed |

Note a grouped mode-01 request drops unsupported PIDs from the reply instead of returning a negative response, so a missing value there is not an error and produces no NRC.

**`fdcan_obd.c`/`.h`** implements the actual OBD-II protocol over FDCAN1 (H743 uses FDCAN, not bxCAN, but this project speaks classic CAN 2.0 frames at 500kbps, not CAN-FD). RX is **interrupt-driven**: `FDCAN1_IT0_IRQHandler` (defined in `fdcan_obd.c`, not `stm32h7xx_it.c` — enabling the FDCAN interrupt in CubeMX would duplicate it) copies every frame from the 16-deep hardware FIFO into a 256-entry ring (7 KB), stamping each with its receive time; `obd_rx_poll()` does all validation and decoding from that ring in the loop. If the notification cannot be enabled, `obd_rx_poll()` drains the FIFO itself (SNIFF shows `POLL`). Ring drops count into `LOST` on DIAG, its deepest fill into `Q`. Live decoded values land in the global `g_obd` (`obd_data_t`), which both displays read from.

These conventions in the OBD path matter and are easy to break:

- **One diagnostic transaction at a time.** `request()` refuses while one is open and `obd_poll_tick()` only ages it. It closes on a valid answer, an NRC for its service (NRC 0x78 extends it, but never past `OBD_TXN_MAX_MS` = 10 s from the request — counted as `CAP` on DIAG), a refused Flow Control, a rejected FF/CF, or its timeout; a multi-frame answer belongs to it and closes only it. A reply only counts as the answer if it **arrived** (ISR receive time) after the request and before the deadline — a late one is still decoded with its own stamp but does not close the slot; in the polled fallback the stamp is only the pump time, so there anything processed before the deadline check counts. A grouped mode-01 request closes only on a reply carrying **every** requested PID (`await_pids`). Don't add a send path that bypasses `request()`.
- **Every received byte is length-checked before it is decoded.** `obd_rx_poll()` checks the DLC for each broadcast, ISO-TP SF/FF/CF lengths, and validates a whole mode-01 reply (known PIDs, exact fit) before writing anything; mode 01 is only accepted from the ECM and each mode-22 DID only from the module it is requested from. Rejected frames increment the `BAD` counter on DIAG. Never decode a missing byte as 0.
- **Read-only, with one bounded exception.** Services used: mode 01, `$22`, and for oil pressure GMLAN `$2C` (define data packet `0xFE` = PID `0xA22C`, volatile ECM RAM, gone at the next ECM reset) + `$AA 01` (send that packet once) — fenced: **off in the default (road) build** (`OILP_DPID_ENABLE 0`: neither is ever sent; the default host suite's TX mock fails on them). Only the KOEO experiment build (`python tools/build.py -D OILP_DPID_ENABLE=1` → `build-oilp_dpid_enable_1/`, host suite `test_oilp_dpid.c`) sends them: `$2C` only with a fresh selector in P **and** a fresh 0 km/h, at most `OILP_DEFINE_MAX` per power-up. Nothing writes calibration/memory, drives an actuator or clears codes (no mode 04, no `$2E`/`$2F`/`$3B`/`$14`). A `0x5E8` UUDT packet is decoded only while our own `$AA` is open — another tester may reuse DPID `0xFE`.
- **Every metric has a freshness stamp; show it only while it is fresh.** Decoders store values through `set_m()`, which stamps `g_obd.upd_ms[k]` with the frame's **receive** time (`rx_ms_cur`, taken in the ISR) on every valid decode, also when the value did not change — never the decode time, or a frame that waited out a stall looks fresh; the selector has `sel_upd_ms`. `obd_is_fresh()` / `obd_sel_fresh()` compare against `metric_stale_ms[]` / `SEL_STALE_MS` in `cluster_config.h` (deliberately generous until `LOOP` is measured). The UI and the status screen show `--` and keep a stale value out of the alert strip — broadcasts keep `can_ok` alive even when the ECM has stopped answering, so `can_ok` alone says nothing about a value. A new metric needs a stamp in its decoder and an entry in `metric_stale_ms`.
- **Every float starts as `NaN`, meaning "the bus has never sent this."** The UI renders NaN as `--`. Do not "fix" a metric by defaulting it to 0 — an unsupported PID would then paint a believable lie (0 V battery, 0 °C oil), which is exactly the bug that once made an entirely dead poller look like a working one.
- **Deadlines are in milliseconds and are checked only in `obd_poll_tick()`, which must run after `obd_rx_poll()` in the same iteration** (`cluster_app_run` does). The loop can still stall (LVGL renders on the CPU; a full repaint is ~200 ms of SPI wire time at 18.75 MHz even with the DMA flush; DIAG shows `LOOP`), but every reply that arrived before the check is already in the RX ring and is processed first, so a stall cannot fake a timeout (host test `stall_does_not_fake_timeout`). Keep that order, and never check a protocol deadline at the moment a frame is decoded. History: with RX drained only from the loop behind a blocking flush, ms deadlines once silently killed every multi-frame reply, which is why they were poll ticks until stage C. MIL/DTC staleness stays counted in unanswered PID 0x01 attempts.

**`cluster_ui.c`/`.h`** builds and refreshes the 3-page LVGL UI (`cluster_ui_build()` once, `cluster_ui_refresh()` at ~25Hz) plus an alert strip; pages are DRIVE(0)/DIAG(1)/SNIFF(2), selected via `cluster_ui_set_page()`/`cluster_ui_next_page()`.

**`can_sniff.c`/`.h`** is a workbench-only passive CAN change detector behind the SNIFF page: it widens the FDCAN acceptance filter to all standard IDs and lists the bytes that changed most recently, so operating one control identifies the frame carrying it. It is fed every accepted standard frame — known IDs (selector, OBD replies, UUDT) included — before `obd_rx_poll()` decodes them, stamped with the receive time. It exists because the signals still missing an identifier (selector range/PRNDL, soot, DPF ∆P, since-regen) are broadcast as ordinary frames — the OEM cluster displayed them. Listening is deliberately tied to the page being visible: the wide filter competes with OBD replies for the same RX FIFO, and `obd_poll_tick()` is suspended while it runs. Because of that it only opens, and stays open, with a fresh selector in **P** and no fresh speed above 3 km/h — not N (it can roll, and speed goes stale while sniffing) — fail closed: CAN lost or a selector never seen / stale means no SNIFF (`policy_sniff_allowed()` in `cluster_policy.h`; `SNIFF_BENCH_OVERRIDE 1` for a bench without a selector, never in the truck).

**`cluster_policy.h`** (header-only) holds the decisions both displays take from a snapshot — `policy_shown()` (link up + value fresh), `policy_ecm_fresh()`, `policy_complete()`, the SNIFF gate, and the alert-strip summary (`NOMINAL` needs RPM, coolant, oil temp, ATF, battery and MIL all fresh; otherwise `PARTIAL DATA`) — so the host tests cover them. Use it rather than re-deriving freshness in a new display path.

**Removed dead code:** the empty `can_gauges.c`/`.h` and `ui_dashboard.c`/`.h` stubs and the unused SNIFF views (`can_sniff_top`/`_get`/`_top_ids`) are gone. CubeIDE drops deleted files on the next Refresh/Build; a stale `Debug/` makefile still listing them needs that refresh before a raw CLI `make`.

**Design reference:** `design/Diesel_Cluster_v2_3.html` is the standalone HTML/JS mockup of the cluster UI (colors, layout, thresholds) that `cluster_ui.c` and `cluster_config.h` were ported from — check it when the visual design intent of a page/metric is unclear from the C code alone. `design/README.md` is currently empty.

## Use graphify before and after non-trivial changes

This project has a graphify knowledge graph in `graphify-out/` (code call graph + community structure). Before making a non-trivial change — anything touching a function/struct used from more than one file, or where the blast radius isn't obvious from a quick grep — run `/graphify query "<question>"` first to see callers, callees, and cross-community connections instead of guessing from a single-file read. After the change, run `/graphify . --update` so the graph stays current for the next session (cheap: only re-extracts changed files, doesn't re-scan the whole project).

Skip this for trivial edits (renaming a constant, fixing a typo, adjusting a threshold value in `cluster_config.h`) where there's no relationship to trace.

## Hardware notes worth knowing before touching related code

- KEY button (PC13) is pulled down and reads **HIGH when pressed** (opposite of many reference designs) — see `key_button_init()` in `app_main.c`. It also raises EXTI15_10 on both edges: the ISR latches presses so a tap that starts and ends inside one (flush-stalled) loop iteration is still counted.
- The onboard heartbeat LED (PE3, active-low) doubles as a backlight-level blink-code indicator once past init — see the comment block above `heartbeat_led_tick()` for the encoding, and above `AppMain_Init()`/`heartbeat_led_init()` for what "off / solid / blinking" mean during boot diagnosis.
- The two displays are fully independent SPI buses/GPIO groups (SPI2 for the main ILI9488 via CubeMX, SPI4 for the ST7735 status screen via hand-written HAL init) and must stay that way — `st7735_status.c`'s header comment explains why it intentionally avoids the `.ioc`.
- ILI9488 has no 16bpp SPI mode; `lv_port_disp.c` expands each flushed area from LVGL's RGB565 into 3-byte RGB666 in `tx_buf`, then sends it by DMA (DMA1 Stream0 -> SPI2 TX) and calls `lv_display_flush_ready()` immediately; the next flush waits for that DMA (bounded at 100 ms). A lost area still calls `flush_ready()`, so `lv_port_disp_service()` (in the loop, before `lv_timer_handler()`) repaints the whole screen after any SPI2 error (≤ 1/s) and re-inits the panel after 3 errors with no good DMA area between (≤ 1 per 5 s, `R` on DIAG). A panel that resets itself on a supply dip gives no SPI error and is not detected. `spi2_dma_init()` in the same file re-inits SPI2 at /8 = 18.75 MHz, raises the PB13-15 pin speed and owns the DMA stream and the `DMA1_Stream0`/`SPI2` IRQ handlers. This is deliberately outside the CubeMX files, so the `.ioc` still says /32 and no DMA; do not enable SPI2 DMA in CubeMX without removing these (the link would fail on duplicate handlers).
- **In LVGL 9 `lv_color_t` is 3 bytes regardless of the display's colour format.** Never size an RGB565 render buffer as `lv_color_t[]`: size it in bytes (`px * 2`). LVGL divides the byte size by the format's stride, so an `lv_color_t[]` buffer silently yields 1.5x taller flush areas; that once overran the DMA buffer and zeroed the HAL handles behind it (watchdog reset loop).
- **Debugging with ST-LINK while the D-cache is on:** debugger reads of SRAM bypass the cache and can show stale data, so trust core and peripheral registers, not RAM variables. IWDG1 PR (`0x58004804`) is a handy reset detector: 6 = boot timeout (just reset), 4 = run mode.
- **I-cache and D-cache are on** (`main.c`, USER CODE Init). All `.data`/`.bss`/stack live in AXI SRAM (cacheable); `.noinit` (the fault record) is in DTCM, which is never cached. The display flush cleans its buffer before each DMA. **Any new DMA must clean (TX) or invalidate (RX) its buffers, 32-byte aligned, or use a non-cacheable MPU region**, otherwise it reads or writes stale data. DMA1/DMA2 cannot reach DTCM at all.
- UI refresh (`cluster_ui_refresh()`, 25 Hz) goes through `ui_text()`/`ui_text_color()`/`ui_bg_color()`/`ui_border_color()`, which skip unchanged values: in LVGL 9.3 `lv_label_set_text` and `lv_obj_set_style_*` invalidate (= repaint over SPI) even when nothing changed. Use them for any new widget updated from the refresh path.

## graphify

This project has a knowledge graph at graphify-out/ with god nodes, community structure, and cross-file relationships.

Rules:
- For codebase questions, first run `graphify query "<question>"` when graphify-out/graph.json exists. Use `graphify path "<A>" "<B>"` for relationships and `graphify explain "<concept>"` for focused concepts. These return a scoped subgraph, usually much smaller than GRAPH_REPORT.md or raw grep output.
- If graphify-out/wiki/index.md exists, use it for broad navigation instead of raw source browsing.
- Read graphify-out/GRAPH_REPORT.md only for broad architecture review or when query/path/explain do not surface enough context.
- After modifying code, run `graphify update .` to keep the graph current (AST-only, no API cost).
