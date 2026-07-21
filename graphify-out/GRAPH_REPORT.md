# Graph Report - Dash  (2026-07-21)

## Corpus Check
- 43 files · ~285,967 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 245 nodes · 380 edges · 35 communities (31 shown, 4 thin omitted)
- Extraction: 90% EXTRACTED · 10% INFERRED · 0% AMBIGUOUS · INFERRED: 38 edges (avg confidence: 0.8)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `2d424a5f`
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
- metric_key_t
- metric_state_t
- SNIFF capture — selector / PRNDL search
- FDCAN_HandleTypeDef
- metric_state_t
- FDCAN_HandleTypeDef

## God Nodes (most connected - your core abstractions)
1. `build_drive()` - 12 edges
2. `mk_label()` - 11 edges
3. `set_metric()` - 11 edges
4. `AppMain_Run()` - 10 edges
5. `mk_value()` - 9 edges
6. `build_dpf()` - 9 edges
7. `cluster_ui_build()` - 8 edges
8. `cluster_ui_refresh()` - 8 edges
9. `obd_on_update()` - 8 edges
10. `AppMain_Init()` - 8 edges

## Surprising Connections (you probably didn't know these)
- `main()` --calls--> `MX_FREERTOS_Init()`  [INFERRED]
  Core/Src/main.c → Core/Src/freertos.c
- `cluster_app_init()` --calls--> `cluster_ui_build()`  [INFERRED]
  Core/Src/cluster_app.c → Core/Src/cluster_ui.c
- `AppMain_Run()` --calls--> `cluster_ui_next_page()`  [INFERRED]
  Core/Src/app_main.c → Core/Src/cluster_ui.c
- `AppMain_Run()` --calls--> `cluster_ui_get_page()`  [INFERRED]
  Core/Src/app_main.c → Core/Src/cluster_ui.c
- `cluster_app_run()` --calls--> `cluster_ui_refresh()`  [INFERRED]
  Core/Src/cluster_app.c → Core/Src/cluster_ui.c

## Import Cycles
- None detected.

## Hyperedges (group relationships)
- **Three-Page Cluster Navigation (Drive/DPF/Diag)** — design_diesel_cluster_v2_3_drive_page, design_diesel_cluster_v2_3_dpf_page, design_diesel_cluster_v2_3_diag_page, design_diesel_cluster_v2_3_tab_switcher [EXTRACTED 1.00]
- **Glanceable Diesel-Health Signature Elements** — design_diesel_cluster_v2_3_systems_rail, design_diesel_cluster_v2_3_calibration_eyebrow, design_diesel_cluster_v2_3_state_colour_encoding, design_diesel_cluster_v2_3_ili9488_constraints [INFERRED 0.75]

## Communities (35 total, 4 thin omitted)

### Community 0 - "FDCAN & OBD Bus Init"
Cohesion: 0.11
Nodes (20): FDCAN_HandleTypeDef, HAL_FDCAN_MspDeInit(), HAL_FDCAN_MspInit(), MX_FDCAN1_Init(), MX_GPIO_Init(), TIM_HandleTypeDef, Error_Handler(), HAL_TIM_PeriodElapsedCallback() (+12 more)

### Community 1 - "Metric Model & App Glue"
Cohesion: 0.25
Nodes (19): cluster_app_run(), obd_on_update(), can_send(), decode_mode01(), decode_mode22(), dispatch(), expect_reply(), obd_check_health() (+11 more)

### Community 2 - "App Main & Superloop"
Cohesion: 0.16
Nodes (21): AppMain_Init(), AppMain_Run(), backlight_set(), heartbeat_led_init(), heartbeat_led_tick(), key_button_init(), cluster_ui_get_page(), MX_FREERTOS_Init() (+13 more)

### Community 3 - "UI Page Builders"
Cohesion: 0.18
Nodes (33): build_diag(), build_dpf(), build_drive(), build_sniff(), build_strip(), cluster_ui_build(), cluster_ui_next_page(), cluster_ui_refresh() (+25 more)

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
Cohesion: 0.13
Nodes (9): apply_filter(), can_sniff_candidates(), can_sniff_movers(), can_sniff_reset(), can_sniff_set_active(), can_sniff_top(), sniff_cand_t, sniff_hit_t (+1 more)

### Community 29 - "metric_key_t"
Cohesion: 0.33
Nodes (5): Added, awaiting on-car confirmation, Confirmed on this vehicle, GM-enhanced DIDs (mode 22) for the Colorado 2.8 / E98, Sources, Still unmapped (candidates to try)

### Community 30 - "metric_state_t"
Cohesion: 0.22
Nodes (7): metric_key_t, metric_state_t, metric_state(), FDCAN_HandleTypeDef, cluster_app_init(), obd_init(), FDCAN_HandleTypeDef

### Community 31 - "SNIFF capture — selector / PRNDL search"
Cohesion: 0.29
Nodes (6): Analysis — distinct value set per interesting byte, Conclusion, Next, Run 1  (design/Photos/1/, frames 023–035), Run 2  (design/Photos/2/, frames 036–052), SNIFF capture — selector / PRNDL search

## Knowledge Gaps
- **34 isolated node(s):** `Confirmed on this vehicle`, `Added, awaiting on-car confirmation`, `Still unmapped (candidates to try)`, `Sources`, `Run 1  (design/Photos/1/, frames 023–035)` (+29 more)
  These have ≤1 connection - possible missing edges or undocumented components.
- **4 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `AppMain_Init()` connect `App Main & Superloop` to `LVGL Display Port`, `metric_state_t`?**
  _High betweenness centrality (0.104) - this node is a cross-community bridge._
- **Why does `AppMain_Run()` connect `App Main & Superloop` to `Metric Model & App Glue`, `UI Page Builders`?**
  _High betweenness centrality (0.099) - this node is a cross-community bridge._
- **Why does `cluster_app_init()` connect `metric_state_t` to `App Main & Superloop`, `UI Page Builders`?**
  _High betweenness centrality (0.078) - this node is a cross-community bridge._
- **Are the 7 inferred relationships involving `AppMain_Run()` (e.g. with `cluster_app_run()` and `cluster_ui_get_page()`) actually correct?**
  _`AppMain_Run()` has 7 INFERRED edges - model-reasoned connections that need verification._
- **What connects `Confirmed on this vehicle`, `Added, awaiting on-car confirmation`, `Still unmapped (candidates to try)` to the rest of the system?**
  _34 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `FDCAN & OBD Bus Init` be split into smaller, more focused modules?**
  _Cohesion score 0.11076923076923077 - nodes in this community are weakly interconnected._
- **Should `UI Refresh & Metric Helpers` be split into smaller, more focused modules?**
  _Cohesion score 0.14285714285714285 - nodes in this community are weakly interconnected._