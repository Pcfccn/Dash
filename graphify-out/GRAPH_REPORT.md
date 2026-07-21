# Graph Report - Dash  (2026-07-21)

## Corpus Check
- 41 files · ~283,024 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 230 nodes · 373 edges · 32 communities (28 shown, 4 thin omitted)
- Extraction: 88% EXTRACTED · 12% INFERRED · 0% AMBIGUOUS · INFERRED: 44 edges (avg confidence: 0.8)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `d0b338dd`
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
- FDCAN_HandleTypeDef

## God Nodes (most connected - your core abstractions)
1. `build_drive()` - 12 edges
2. `mk_label()` - 11 edges
3. `set_metric()` - 11 edges
4. `cluster_ui_refresh()` - 10 edges
5. `AppMain_Run()` - 10 edges
6. `mk_value()` - 9 edges
7. `build_dpf()` - 9 edges
8. `cluster_ui_build()` - 8 edges
9. `AppMain_Init()` - 8 edges
10. `main()` - 8 edges

## Surprising Connections (you probably didn't know these)
- `obd_poll_tick()` --calls--> `can_sniff_is_active()`  [INFERRED]
  Core/Src/fdcan_obd.c → Core/Src/can_sniff.c
- `cluster_ui_refresh()` --calls--> `can_sniff_fps()`  [INFERRED]
  Core/Src/cluster_ui.c → Core/Src/can_sniff.c
- `cluster_ui_set_page()` --calls--> `can_sniff_set_active()`  [INFERRED]
  Core/Src/cluster_ui.c → Core/Src/can_sniff.c
- `cluster_ui_refresh()` --calls--> `can_sniff_id_count()`  [INFERRED]
  Core/Src/cluster_ui.c → Core/Src/can_sniff.c
- `obd_rx_poll()` --calls--> `can_sniff_feed()`  [INFERRED]
  Core/Src/fdcan_obd.c → Core/Src/can_sniff.c

## Import Cycles
- None detected.

## Hyperedges (group relationships)
- **Three-Page Cluster Navigation (Drive/DPF/Diag)** — design_diesel_cluster_v2_3_drive_page, design_diesel_cluster_v2_3_dpf_page, design_diesel_cluster_v2_3_diag_page, design_diesel_cluster_v2_3_tab_switcher [EXTRACTED 1.00]
- **Glanceable Diesel-Health Signature Elements** — design_diesel_cluster_v2_3_systems_rail, design_diesel_cluster_v2_3_calibration_eyebrow, design_diesel_cluster_v2_3_state_colour_encoding, design_diesel_cluster_v2_3_ili9488_constraints [INFERRED 0.75]

## Communities (32 total, 4 thin omitted)

### Community 0 - "FDCAN & OBD Bus Init"
Cohesion: 0.11
Nodes (22): FDCAN_HandleTypeDef, HAL_FDCAN_MspDeInit(), HAL_FDCAN_MspInit(), MX_FDCAN1_Init(), MX_FREERTOS_Init(), StartDefaultTask(), MX_GPIO_Init(), TIM_HandleTypeDef (+14 more)

### Community 1 - "Metric Model & App Glue"
Cohesion: 0.13
Nodes (27): metric_key_t, metric_state_t, metric_state(), can_sniff_feed(), FDCAN_HandleTypeDef, cluster_app_init(), cluster_app_run(), obd_on_update() (+19 more)

### Community 2 - "App Main & Superloop"
Cohesion: 0.19
Nodes (19): AppMain_Init(), AppMain_Run(), backlight_set(), heartbeat_led_init(), heartbeat_led_tick(), key_button_init(), cluster_ui_get_page(), lv_display_t (+11 more)

### Community 3 - "UI Page Builders"
Cohesion: 0.17
Nodes (34): can_sniff_fps(), can_sniff_id_count(), build_diag(), build_dpf(), build_drive(), build_sniff(), build_strip(), cluster_ui_build() (+26 more)

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
Cohesion: 0.20
Nodes (8): apply_filter(), can_sniff_candidates(), can_sniff_is_active(), can_sniff_reset(), can_sniff_set_active(), can_sniff_top(), sniff_cand_t, sniff_hit_t

## Knowledge Gaps
- **26 isolated node(s):** `What this is`, `Git workflow: commit and push automatically`, `Build / flash / debug`, `Architecture`, `Use graphify before and after non-trivial changes` (+21 more)
  These have ≤1 connection - possible missing edges or undocumented components.
- **4 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `AppMain_Run()` connect `App Main & Superloop` to `FDCAN & OBD Bus Init`, `Metric Model & App Glue`, `UI Page Builders`?**
  _High betweenness centrality (0.114) - this node is a cross-community bridge._
- **Why does `AppMain_Init()` connect `App Main & Superloop` to `FDCAN & OBD Bus Init`, `Metric Model & App Glue`, `LVGL Display Port`?**
  _High betweenness centrality (0.113) - this node is a cross-community bridge._
- **Why does `cluster_app_init()` connect `Metric Model & App Glue` to `App Main & Superloop`, `UI Page Builders`?**
  _High betweenness centrality (0.083) - this node is a cross-community bridge._
- **Are the 4 inferred relationships involving `cluster_ui_refresh()` (e.g. with `cluster_app_run()` and `can_sniff_candidates()`) actually correct?**
  _`cluster_ui_refresh()` has 4 INFERRED edges - model-reasoned connections that need verification._
- **What connects `What this is`, `Git workflow: commit and push automatically`, `Build / flash / debug` to the rest of the system?**
  _26 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `FDCAN & OBD Bus Init` be split into smaller, more focused modules?**
  _Cohesion score 0.10591133004926108 - nodes in this community are weakly interconnected._
- **Should `Metric Model & App Glue` be split into smaller, more focused modules?**
  _Cohesion score 0.12903225806451613 - nodes in this community are weakly interconnected._