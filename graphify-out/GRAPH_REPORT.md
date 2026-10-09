# Graph Report - Dash  (2026-10-09)

## Corpus Check
- 50 files · ~737,360 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 291 nodes · 417 edges · 35 communities (33 shown, 2 thin omitted)
- Extraction: 90% EXTRACTED · 10% INFERRED · 0% AMBIGUOUS · INFERRED: 40 edges (avg confidence: 0.8)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `4458417a`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- FDCAN & OBD Bus Init
- Metric Model & App Glue
- App Main & Superloop
- UI Page Builders
- LVGL Display Port
- Cluster Design Spec
- UI Refresh & Metric Helpers
- HAL Timebase
- Metric Key Enum
- Metric State Enum
- set_metric
- SNIFF page (STATE + ANALOG)
- metric_state_t
- SNIFF capture — selector / PRNDL search
- Сессия 2026-07-24, утро (~10:19–10:20) — расшифровка фото
- Oil-pressure source test (throttle-blip correlation)
- Findings

## God Nodes (most connected - your core abstractions)
1. `set_metric()` - 12 edges
2. `cluster_ui_refresh()` - 12 edges
3. `GPS-скорость и замер разгона («свой Dragy»)` - 11 edges
4. `build_drive()` - 11 edges
5. `AppMain_Run()` - 10 edges
6. `mk_label()` - 10 edges
7. `AppMain_Init()` - 8 edges
8. `mk_value()` - 8 edges
9. `main()` - 8 edges
10. `Error_Handler()` - 8 edges

## Surprising Connections (you probably didn't know these)
- `cluster_ui_refresh()` --calls--> `obd_can_health()`  [INFERRED]
  Core/Src/cluster_ui.c → Core/Src/fdcan_obd.c
- `cluster_app_run()` --calls--> `obd_watchdog_tick_1hz()`  [INFERRED]
  Core/Src/cluster_app.c → Core/Src/fdcan_obd.c
- `cluster_app_run()` --calls--> `obd_demo_tick()`  [INFERRED]
  Core/Src/cluster_app.c → Core/Src/fdcan_obd.c
- `set_metric()` --calls--> `metric_state()`  [INFERRED]
  Core/Src/cluster_ui.c → Core/Inc/cluster_config.h
- `AppMain_Init()` --calls--> `cluster_app_init()`  [INFERRED]
  Core/Src/app_main.c → Core/Src/cluster_app.c

## Import Cycles
- None detected.

## Hyperedges (group relationships)
- **Three-Page Cluster Navigation (Drive/DPF/Diag)** — design_diesel_cluster_v2_3_drive_page, design_diesel_cluster_v2_3_dpf_page, design_diesel_cluster_v2_3_diag_page, design_diesel_cluster_v2_3_tab_switcher [EXTRACTED 1.00]
- **Glanceable Diesel-Health Signature Elements** — design_diesel_cluster_v2_3_systems_rail, design_diesel_cluster_v2_3_calibration_eyebrow, design_diesel_cluster_v2_3_state_colour_encoding, design_diesel_cluster_v2_3_ili9488_constraints [INFERRED 0.75]

## Communities (35 total, 2 thin omitted)

### Community 0 - "FDCAN & OBD Bus Init"
Cohesion: 0.10
Nodes (22): FDCAN_HandleTypeDef, HAL_FDCAN_MspDeInit(), HAL_FDCAN_MspInit(), MX_FDCAN1_Init(), MX_FREERTOS_Init(), StartDefaultTask(), MX_GPIO_Init(), TIM_HandleTypeDef (+14 more)

### Community 1 - "Metric Model & App Glue"
Cohesion: 0.25
Nodes (18): cluster_app_run(), can_send(), decode_mode01(), decode_mode22(), dispatch(), expect_reply(), obd_check_health(), obd_demo_tick() (+10 more)

### Community 2 - "App Main & Superloop"
Cohesion: 0.19
Nodes (19): AppMain_Init(), AppMain_Run(), backlight_set(), heartbeat_led_init(), heartbeat_led_tick(), key_button_init(), cluster_ui_get_page(), lv_display_t (+11 more)

### Community 3 - "UI Page Builders"
Cohesion: 0.18
Nodes (29): build_diag(), build_drive(), build_sniff(), build_strip(), metric_key_t, metric_state_t, cluster_ui_build(), cluster_ui_next_page() (+21 more)

### Community 4 - "LVGL Display Port"
Cohesion: 0.33
Nodes (13): lv_display_t, cs_high(), cs_low(), dc_cmd(), dc_data(), disp_flush_cb(), ili9488_init_sequence(), lcd_set_window() (+5 more)

### Community 5 - "Cluster Design Spec"
Cohesion: 0.22
Nodes (13): Calibration Eyebrow, Colorado 2.8 Diesel Cluster v2 Mockup, Holden Colorado RG 2.8 E98 ECM, DIAG Page, DPF Page, DPF Soot / Regeneration Monitoring, DRIVE Page, ECU Instrument Direction (+5 more)

### Community 6 - "UI Refresh & Metric Helpers"
Cohesion: 0.14
Nodes (13): Backlight control module, Boot / programming, CAN transceiver module, Core board, External modules (as purchased), Hardware reference — WeAct MiniSTM32H743, Main 4" display module, Onboard 0.96" ST7735 status display (SPI4) (+5 more)

### Community 22 - "Metric Key Enum"
Cohesion: 0.22
Nodes (7): Architecture, Build / flash / debug, Git workflow: commit and push automatically, graphify, Hardware notes worth knowing before touching related code, Use graphify before and after non-trivial changes, What this is

### Community 23 - "Metric State Enum"
Cohesion: 0.15
Nodes (12): 1. Powering the board in the car, 2. Connecting to the CAN bus (OBD-II port), 3. Read-only safety, 4. Bring-up sequence, 5. Known gaps for a "complete" in-car build, Appendix: full connection & power map, ⚠️ Bus termination — the #1 thing to get right, In-vehicle install — from bench demo to a live car (+4 more)

### Community 27 - "set_metric"
Cohesion: 0.08
Nodes (21): metric_key_t, metric_state_t, metric_state(), apply_filter(), can_sniff_candidates(), can_sniff_fps(), can_sniff_id_count(), can_sniff_movers() (+13 more)

### Community 29 - "SNIFF page (STATE + ANALOG)"
Cohesion: 0.11
Nodes (18): 1. Зачем GPS, если есть OBD-скорость, 2. Аналоги, 3. Выбор GNSS-чипа, 4. Интеграция в текущую систему, 5. План прошивки и трудозатраты, 6. Подводные камни, 7. Резервное питание модуля: не нужно, 8. Железо: варианты и BOM (+10 more)

### Community 30 - "metric_state_t"
Cohesion: 0.20
Nodes (9): Анализ: что ответили новые enhanced-DID, Выводы / что дальше, КРИТИЧНО: скорость/обороты/rail/batt/load не читались, Прогон 2026-07-21 (после ~5 мин поездки, снимок на парковке), Расшифровка, Фото 1–4 — SNIFF (широковещательные кадры, мотор заглушён), Фото 5 — DIAG, Фото 6 — DPF (+1 more)

### Community 31 - "SNIFF capture — selector / PRNDL search"
Cohesion: 0.13
Nodes (13): Added, awaiting on-car confirmation, Being probed on the car (standard diesel PIDs), Confirmed on this vehicle, GM-enhanced DIDs (mode 22) for the Colorado 2.8 / E98, Oil pressure — mode 22 confirmed absent on this E98, Oil pressure via passive CAN broadcast, Sources, Analysis — distinct value set per interesting byte (+5 more)

### Community 33 - "Oil-pressure source test (throttle-blip correlation)"
Cohesion: 0.25
Nodes (7): After the test, Oil-pressure source test (throttle-blip correlation), Procedure, Reading the photos, The principle, What the firmware now shows (DIAG page), Why

### Community 34 - "Findings"
Cohesion: 0.20
Nodes (9): 1. OIL P (oil pressure) — candidate 0x1BA[3] is NOT oil pressure — REJECT, 2. SPEED — ~10 s lag + ~12 % high, 3. OIL temp — plausible magnitude, trend to verify, 4. Reads OK (no action), DIAG page (photos 17–18), DRIVE page readings (photos 19–26), Findings, Road-test session — 2026-08-23 (26 photos) (+1 more)

## Knowledge Gaps
- **70 isolated node(s):** `DRIVE page readings (photos 19–26)`, `DIAG page (photos 17–18)`, `1. OIL P (oil pressure) — candidate 0x1BA[3] is NOT oil pressure — REJECT`, `2. SPEED — ~10 s lag + ~12 % high`, `3. OIL temp — plausible magnitude, trend to verify` (+65 more)
  These have ≤1 connection - possible missing edges or undocumented components.
- **2 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `AppMain_Run()` connect `App Main & Superloop` to `FDCAN & OBD Bus Init`, `Metric Model & App Glue`, `UI Page Builders`?**
  _High betweenness centrality (0.074) - this node is a cross-community bridge._
- **Why does `AppMain_Init()` connect `App Main & Superloop` to `FDCAN & OBD Bus Init`, `set_metric`, `LVGL Display Port`?**
  _High betweenness centrality (0.069) - this node is a cross-community bridge._
- **Why does `cluster_app_init()` connect `set_metric` to `App Main & Superloop`, `UI Page Builders`?**
  _High betweenness centrality (0.050) - this node is a cross-community bridge._
- **Are the 7 inferred relationships involving `cluster_ui_refresh()` (e.g. with `cluster_app_run()` and `metric_state()`) actually correct?**
  _`cluster_ui_refresh()` has 7 INFERRED edges - model-reasoned connections that need verification._
- **What connects `DRIVE page readings (photos 19–26)`, `DIAG page (photos 17–18)`, `1. OIL P (oil pressure) — candidate 0x1BA[3] is NOT oil pressure — REJECT` to the rest of the system?**
  _70 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `FDCAN & OBD Bus Init` be split into smaller, more focused modules?**
  _Cohesion score 0.10344827586206896 - nodes in this community are weakly interconnected._
- **Should `UI Refresh & Metric Helpers` be split into smaller, more focused modules?**
  _Cohesion score 0.14285714285714285 - nodes in this community are weakly interconnected._