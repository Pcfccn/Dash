# Graph Report - Dash  (2026-07-21)

## Corpus Check
- 42 files · ~285,207 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 239 nodes · 384 edges · 33 communities (29 shown, 4 thin omitted)
- Extraction: 88% EXTRACTED · 12% INFERRED · 0% AMBIGUOUS · INFERRED: 47 edges (avg confidence: 0.8)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `1446ba17`
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

## God Nodes (most connected - your core abstractions)
1. `cluster_ui_refresh()` - 13 edges
2. `build_drive()` - 12 edges
3. `set_metric()` - 12 edges
4. `mk_label()` - 11 edges
5. `AppMain_Run()` - 10 edges
6. `mk_value()` - 9 edges
7. `build_dpf()` - 9 edges
8. `cluster_ui_build()` - 8 edges
9. `obd_on_update()` - 8 edges
10. `obd_rx_poll()` - 8 edges

## Surprising Connections (you probably didn't know these)
- `obd_poll_tick()` --calls--> `can_sniff_is_active()`  [INFERRED]
  Core/Src/fdcan_obd.c → Core/Src/can_sniff.c
- `obd_rx_poll()` --calls--> `can_sniff_feed()`  [INFERRED]
  Core/Src/fdcan_obd.c → Core/Src/can_sniff.c
- `set_metric()` --calls--> `metric_state()`  [INFERRED]
  Core/Src/cluster_ui.c → Core/Inc/cluster_config.h
- `cluster_ui_refresh()` --calls--> `can_sniff_fps()`  [INFERRED]
  Core/Src/cluster_ui.c → Core/Src/can_sniff.c
- `cluster_ui_set_page()` --calls--> `can_sniff_set_active()`  [INFERRED]
  Core/Src/cluster_ui.c → Core/Src/can_sniff.c

## Import Cycles
- None detected.

## Hyperedges (group relationships)
- **Three-Page Cluster Navigation (Drive/DPF/Diag)** — design_diesel_cluster_v2_3_drive_page, design_diesel_cluster_v2_3_dpf_page, design_diesel_cluster_v2_3_diag_page, design_diesel_cluster_v2_3_tab_switcher [EXTRACTED 1.00]
- **Glanceable Diesel-Health Signature Elements** — design_diesel_cluster_v2_3_systems_rail, design_diesel_cluster_v2_3_calibration_eyebrow, design_diesel_cluster_v2_3_state_colour_encoding, design_diesel_cluster_v2_3_ili9488_constraints [INFERRED 0.75]

## Communities (33 total, 4 thin omitted)

### Community 0 - "FDCAN & OBD Bus Init"
Cohesion: 0.10
Nodes (22): FDCAN_HandleTypeDef, HAL_FDCAN_MspDeInit(), HAL_FDCAN_MspInit(), MX_FDCAN1_Init(), MX_FREERTOS_Init(), StartDefaultTask(), MX_GPIO_Init(), TIM_HandleTypeDef (+14 more)

### Community 1 - "Metric Model & App Glue"
Cohesion: 0.18
Nodes (23): FDCAN_HandleTypeDef, cluster_app_init(), cluster_app_run(), obd_on_update(), FDCAN_HandleTypeDef, can_send(), decode_mode01(), decode_mode22() (+15 more)

### Community 2 - "App Main & Superloop"
Cohesion: 0.19
Nodes (19): AppMain_Init(), AppMain_Run(), backlight_set(), heartbeat_led_init(), heartbeat_led_tick(), key_button_init(), cluster_ui_get_page(), lv_display_t (+11 more)

### Community 3 - "UI Page Builders"
Cohesion: 0.20
Nodes (30): build_diag(), build_dpf(), build_drive(), build_sniff(), build_strip(), metric_key_t, metric_state_t, cluster_ui_build() (+22 more)

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
Cohesion: 0.11
Nodes (19): metric_key_t, metric_state_t, metric_state(), apply_filter(), can_sniff_candidates(), can_sniff_feed(), can_sniff_fps(), can_sniff_id_count() (+11 more)

### Community 31 - "SNIFF capture — selector / PRNDL search"
Cohesion: 0.29
Nodes (6): Analysis — distinct value set per interesting byte, Conclusion, Next, Run 1  (design/Photos/1/, frames 023–035), Run 2  (design/Photos/2/, frames 036–052), SNIFF capture — selector / PRNDL search

## Knowledge Gaps
- **30 isolated node(s):** `Run 1  (design/Photos/1/, frames 023–035)`, `Run 2  (design/Photos/2/, frames 036–052)`, `Conclusion`, `Next`, `What this is` (+25 more)
  These have ≤1 connection - possible missing edges or undocumented components.
- **4 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `AppMain_Run()` connect `App Main & Superloop` to `FDCAN & OBD Bus Init`, `Metric Model & App Glue`, `UI Page Builders`?**
  _High betweenness centrality (0.117) - this node is a cross-community bridge._
- **Why does `AppMain_Init()` connect `App Main & Superloop` to `FDCAN & OBD Bus Init`, `Metric Model & App Glue`, `LVGL Display Port`?**
  _High betweenness centrality (0.099) - this node is a cross-community bridge._
- **Why does `StartDefaultTask()` connect `FDCAN & OBD Bus Init` to `App Main & Superloop`?**
  _High betweenness centrality (0.070) - this node is a cross-community bridge._
- **Are the 7 inferred relationships involving `cluster_ui_refresh()` (e.g. with `cluster_app_run()` and `metric_state()`) actually correct?**
  _`cluster_ui_refresh()` has 7 INFERRED edges - model-reasoned connections that need verification._
- **What connects `Run 1  (design/Photos/1/, frames 023–035)`, `Run 2  (design/Photos/2/, frames 036–052)`, `Conclusion` to the rest of the system?**
  _30 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `FDCAN & OBD Bus Init` be split into smaller, more focused modules?**
  _Cohesion score 0.10098522167487685 - nodes in this community are weakly interconnected._
- **Should `UI Refresh & Metric Helpers` be split into smaller, more focused modules?**
  _Cohesion score 0.14285714285714285 - nodes in this community are weakly interconnected._