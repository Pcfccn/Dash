# Graph Report - .  (2026-07-19)

## Corpus Check
- 2 files · ~277,128 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 168 nodes · 293 edges · 26 communities (23 shown, 3 thin omitted)
- Extraction: 88% EXTRACTED · 12% INFERRED · 0% AMBIGUOUS · INFERRED: 36 edges (avg confidence: 0.8)
- Token cost: 0 input · 0 output

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
2. `set_metric()` - 11 edges
3. `AppMain_Run()` - 10 edges
4. `mk_label()` - 10 edges
5. `mk_value()` - 9 edges
6. `build_dpf()` - 9 edges
7. `AppMain_Init()` - 8 edges
8. `main()` - 8 edges
9. `Error_Handler()` - 8 edges
10. `cluster_app_run()` - 7 edges

## Surprising Connections (you probably didn't know these)
- `AppMain_Init()` --calls--> `cluster_app_init()`  [INFERRED]
  Core/Src/app_main.c → Core/Src/cluster_app.c
- `AppMain_Init()` --calls--> `lv_port_disp_init()`  [INFERRED]
  Core/Src/app_main.c → Core/Src/lv_port_disp.c
- `StartDefaultTask()` --calls--> `AppMain_Init()`  [INFERRED]
  Core/Src/freertos.c → Core/Src/app_main.c
- `AppMain_Run()` --calls--> `cluster_app_run()`  [INFERRED]
  Core/Src/app_main.c → Core/Src/cluster_app.c
- `AppMain_Run()` --calls--> `cluster_ui_get_page()`  [INFERRED]
  Core/Src/app_main.c → Core/Src/cluster_ui.c

## Import Cycles
- None detected.

## Hyperedges (group relationships)
- **Three-Page Cluster Navigation (Drive/DPF/Diag)** — design_diesel_cluster_v2_3_drive_page, design_diesel_cluster_v2_3_dpf_page, design_diesel_cluster_v2_3_diag_page, design_diesel_cluster_v2_3_tab_switcher [EXTRACTED 1.00]
- **Glanceable Diesel-Health Signature Elements** — design_diesel_cluster_v2_3_systems_rail, design_diesel_cluster_v2_3_calibration_eyebrow, design_diesel_cluster_v2_3_state_colour_encoding, design_diesel_cluster_v2_3_ili9488_constraints [INFERRED 0.75]

## Communities (26 total, 3 thin omitted)

### Community 0 - "FDCAN & OBD Bus Init"
Cohesion: 0.11
Nodes (22): FDCAN_HandleTypeDef, HAL_FDCAN_MspDeInit(), HAL_FDCAN_MspInit(), MX_FDCAN1_Init(), MX_FREERTOS_Init(), StartDefaultTask(), MX_GPIO_Init(), TIM_HandleTypeDef (+14 more)

### Community 1 - "Metric Model & App Glue"
Cohesion: 0.15
Nodes (20): metric_key_t, metric_state_t, metric_state(), FDCAN_HandleTypeDef, cluster_app_init(), cluster_app_run(), obd_on_update(), FDCAN_HandleTypeDef (+12 more)

### Community 2 - "App Main & Superloop"
Cohesion: 0.19
Nodes (19): AppMain_Init(), AppMain_Run(), backlight_set(), heartbeat_led_init(), heartbeat_led_tick(), key_button_init(), cluster_ui_get_page(), lv_display_t (+11 more)

### Community 3 - "UI Page Builders"
Cohesion: 0.33
Nodes (19): build_diag(), build_dpf(), build_drive(), build_strip(), cluster_ui_build(), cluster_ui_next_page(), cluster_ui_set_page(), mk_bar() (+11 more)

### Community 4 - "LVGL Display Port"
Cohesion: 0.33
Nodes (13): lv_display_t, cs_high(), cs_low(), dc_cmd(), dc_data(), disp_flush_cb(), ili9488_init_sequence(), lcd_set_window() (+5 more)

### Community 5 - "Cluster Design Spec"
Cohesion: 0.22
Nodes (13): Calibration Eyebrow, Colorado 2.8 Diesel Cluster v2 Mockup, Holden Colorado RG 2.8 E98 ECM, DIAG Page, DPF Page, DPF Soot / Regeneration Monitoring, DRIVE Page, ECU Instrument Direction (+5 more)

### Community 6 - "UI Refresh & Metric Helpers"
Cohesion: 0.32
Nodes (12): cluster_ui_refresh(), fmt(), iround(), is_enhanced(), is_temp(), mval(), pct_of(), set_metric() (+4 more)

## Knowledge Gaps
- **3 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `AppMain_Init()` connect `App Main & Superloop` to `FDCAN & OBD Bus Init`, `Metric Model & App Glue`, `LVGL Display Port`?**
  _High betweenness centrality (0.168) - this node is a cross-community bridge._
- **Why does `AppMain_Run()` connect `App Main & Superloop` to `FDCAN & OBD Bus Init`, `Metric Model & App Glue`, `UI Page Builders`?**
  _High betweenness centrality (0.155) - this node is a cross-community bridge._
- **Why does `Error_Handler()` connect `FDCAN & OBD Bus Init` to `App Main & Superloop`?**
  _High betweenness centrality (0.117) - this node is a cross-community bridge._
- **Are the 7 inferred relationships involving `AppMain_Run()` (e.g. with `cluster_app_run()` and `cluster_ui_get_page()`) actually correct?**
  _`AppMain_Run()` has 7 INFERRED edges - model-reasoned connections that need verification._
- **Should `FDCAN & OBD Bus Init` be split into smaller, more focused modules?**
  _Cohesion score 0.10591133004926108 - nodes in this community are weakly interconnected._