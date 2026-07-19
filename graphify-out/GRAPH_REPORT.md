# Graph Report - Dash  (2026-07-19)

## Corpus Check
- 39 files · ~35,009 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 204 nodes · 334 edges · 27 communities (26 shown, 1 thin omitted)
- Extraction: 88% EXTRACTED · 12% INFERRED · 0% AMBIGUOUS · INFERRED: 39 edges (avg confidence: 0.8)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `8b0a2b7f`
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

## God Nodes (most connected - your core abstractions)
1. `build_drive()` - 12 edges
2. `set_metric()` - 12 edges
3. `AppMain_Run()` - 10 edges
4. `mk_label()` - 10 edges
5. `mk_value()` - 9 edges
6. `build_dpf()` - 9 edges
7. `AppMain_Init()` - 8 edges
8. `cluster_ui_refresh()` - 8 edges
9. `main()` - 8 edges
10. `Error_Handler()` - 8 edges

## Surprising Connections (you probably didn't know these)
- `cluster_ui_refresh()` --calls--> `metric_state()`  [INFERRED]
  Core/Src/cluster_ui.c → Core/Inc/cluster_config.h
- `set_metric()` --calls--> `metric_state()`  [INFERRED]
  Core/Src/cluster_ui.c → Core/Inc/cluster_config.h
- `AppMain_Init()` --calls--> `cluster_app_init()`  [INFERRED]
  Core/Src/app_main.c → Core/Src/cluster_app.c
- `AppMain_Init()` --calls--> `lv_port_disp_init()`  [INFERRED]
  Core/Src/app_main.c → Core/Src/lv_port_disp.c
- `StartDefaultTask()` --calls--> `AppMain_Init()`  [INFERRED]
  Core/Src/freertos.c → Core/Src/app_main.c

## Import Cycles
- None detected.

## Hyperedges (group relationships)
- **Three-Page Cluster Navigation (Drive/DPF/Diag)** — design_diesel_cluster_v2_3_drive_page, design_diesel_cluster_v2_3_dpf_page, design_diesel_cluster_v2_3_diag_page, design_diesel_cluster_v2_3_tab_switcher [EXTRACTED 1.00]
- **Glanceable Diesel-Health Signature Elements** — design_diesel_cluster_v2_3_systems_rail, design_diesel_cluster_v2_3_calibration_eyebrow, design_diesel_cluster_v2_3_state_colour_encoding, design_diesel_cluster_v2_3_ili9488_constraints [INFERRED 0.75]

## Communities (27 total, 1 thin omitted)

### Community 0 - "FDCAN & OBD Bus Init"
Cohesion: 0.11
Nodes (22): FDCAN_HandleTypeDef, HAL_FDCAN_MspDeInit(), HAL_FDCAN_MspInit(), MX_FDCAN1_Init(), MX_FREERTOS_Init(), StartDefaultTask(), MX_GPIO_Init(), TIM_HandleTypeDef (+14 more)

### Community 1 - "Metric Model & App Glue"
Cohesion: 0.15
Nodes (22): metric_key_t, metric_state_t, metric_state(), FDCAN_HandleTypeDef, cluster_app_init(), cluster_app_run(), obd_on_update(), FDCAN_HandleTypeDef (+14 more)

### Community 2 - "App Main & Superloop"
Cohesion: 0.19
Nodes (19): AppMain_Init(), AppMain_Run(), backlight_set(), heartbeat_led_init(), heartbeat_led_tick(), key_button_init(), cluster_ui_get_page(), lv_display_t (+11 more)

### Community 3 - "UI Page Builders"
Cohesion: 0.19
Nodes (31): build_diag(), build_dpf(), build_drive(), build_strip(), metric_key_t, metric_state_t, cluster_ui_build(), cluster_ui_next_page() (+23 more)

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
Cohesion: 0.17
Nodes (11): 1. Powering the board in the car, 2. Connecting to the CAN bus (OBD-II port), 3. Read-only safety, 4. Bring-up sequence, 5. Known gaps for a "complete" in-car build, Appendix: full connection & power map, ⚠️ Bus termination — the #1 thing to get right, In-vehicle install — from bench demo to a live car (+3 more)

## Knowledge Gaps
- **25 isolated node(s):** `What this is`, `Git workflow: commit and push automatically`, `Build / flash / debug`, `Architecture`, `Use graphify before and after non-trivial changes` (+20 more)
  These have ≤1 connection - possible missing edges or undocumented components.
- **1 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `AppMain_Init()` connect `App Main & Superloop` to `FDCAN & OBD Bus Init`, `Metric Model & App Glue`, `LVGL Display Port`?**
  _High betweenness centrality (0.114) - this node is a cross-community bridge._
- **Why does `AppMain_Run()` connect `App Main & Superloop` to `FDCAN & OBD Bus Init`, `Metric Model & App Glue`, `UI Page Builders`?**
  _High betweenness centrality (0.111) - this node is a cross-community bridge._
- **Why does `Error_Handler()` connect `FDCAN & OBD Bus Init` to `App Main & Superloop`?**
  _High betweenness centrality (0.080) - this node is a cross-community bridge._
- **Are the 7 inferred relationships involving `AppMain_Run()` (e.g. with `cluster_app_run()` and `cluster_ui_get_page()`) actually correct?**
  _`AppMain_Run()` has 7 INFERRED edges - model-reasoned connections that need verification._
- **What connects `What this is`, `Git workflow: commit and push automatically`, `Build / flash / debug` to the rest of the system?**
  _25 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `FDCAN & OBD Bus Init` be split into smaller, more focused modules?**
  _Cohesion score 0.10591133004926108 - nodes in this community are weakly interconnected._
- **Should `UI Refresh & Metric Helpers` be split into smaller, more focused modules?**
  _Cohesion score 0.14285714285714285 - nodes in this community are weakly interconnected._