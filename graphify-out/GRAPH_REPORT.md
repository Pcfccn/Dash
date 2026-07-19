# Graph Report - .  (2026-07-19)

## Corpus Check
- Corpus is ~18,742 words - fits in a single context window. You may not need a graph.

## Summary
- 164 nodes · 286 edges · 24 communities (23 shown, 1 thin omitted)
- Extraction: 87% EXTRACTED · 13% INFERRED · 0% AMBIGUOUS · INFERRED: 38 edges (avg confidence: 0.8)
- Token cost: 28,840 input · 0 output

## Community Hubs (Navigation)
- System & Peripheral Init
- App Main & Status Display
- OBD-II CAN Decoding
- UI Widget Builders
- Metric State & Refresh
- LVGL Display Port Driver
- Cluster Design Spec
- Timer Peripheral
- HAL Timebase

## God Nodes (most connected - your core abstractions)
1. `set_metric()` - 12 edges
2. `build_drive()` - 11 edges
3. `AppMain_Run()` - 10 edges
4. `mk_label()` - 9 edges
5. `AppMain_Init()` - 8 edges
6. `build_dpf()` - 8 edges
7. `cluster_ui_refresh()` - 8 edges
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

## Communities (24 total, 1 thin omitted)

### Community 0 - "System & Peripheral Init"
Cohesion: 0.13
Nodes (17): FDCAN_HandleTypeDef, HAL_FDCAN_MspDeInit(), HAL_FDCAN_MspInit(), MX_FDCAN1_Init(), MX_FREERTOS_Init(), StartDefaultTask(), MX_GPIO_Init(), TIM_HandleTypeDef (+9 more)

### Community 1 - "App Main & Status Display"
Cohesion: 0.19
Nodes (19): AppMain_Init(), AppMain_Run(), backlight_set(), heartbeat_led_init(), heartbeat_led_tick(), key_button_init(), cluster_ui_get_page(), lv_display_t (+11 more)

### Community 2 - "OBD-II CAN Decoding"
Cohesion: 0.19
Nodes (17): FDCAN_HandleTypeDef, cluster_app_init(), cluster_app_run(), obd_on_update(), FDCAN_HandleTypeDef, can_send(), decode_mode01(), dispatch() (+9 more)

### Community 3 - "UI Widget Builders"
Cohesion: 0.32
Nodes (18): build_diag(), build_dpf(), build_drive(), build_strip(), cluster_ui_build(), cluster_ui_next_page(), cluster_ui_set_page(), mk_bar() (+10 more)

### Community 4 - "Metric State & Refresh"
Cohesion: 0.24
Nodes (15): metric_key_t, metric_state_t, metric_state(), metric_key_t, metric_state_t, cluster_ui_refresh(), fmt(), iround() (+7 more)

### Community 5 - "LVGL Display Port Driver"
Cohesion: 0.33
Nodes (13): lv_display_t, cs_high(), cs_low(), dc_cmd(), dc_data(), disp_flush_cb(), ili9488_init_sequence(), lcd_set_window() (+5 more)

### Community 6 - "Cluster Design Spec"
Cohesion: 0.22
Nodes (13): Calibration Eyebrow, Colorado 2.8 Diesel Cluster v2 Mockup, Holden Colorado RG 2.8 E98 ECM, DIAG Page, DPF Page, DPF Soot / Regeneration Monitoring, DRIVE Page, ECU Instrument Direction (+5 more)

### Community 8 - "Timer Peripheral"
Cohesion: 0.53
Nodes (5): TIM_HandleTypeDef, HAL_TIM_MspPostInit(), HAL_TIM_PWM_MspDeInit(), HAL_TIM_PWM_MspInit(), MX_TIM1_Init()

## Knowledge Gaps
- **1 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `AppMain_Init()` connect `App Main & Status Display` to `System & Peripheral Init`, `OBD-II CAN Decoding`, `LVGL Display Port Driver`?**
  _High betweenness centrality (0.171) - this node is a cross-community bridge._
- **Why does `AppMain_Run()` connect `App Main & Status Display` to `System & Peripheral Init`, `OBD-II CAN Decoding`, `UI Widget Builders`?**
  _High betweenness centrality (0.165) - this node is a cross-community bridge._
- **Why does `Error_Handler()` connect `System & Peripheral Init` to `Timer Peripheral`, `App Main & Status Display`?**
  _High betweenness centrality (0.121) - this node is a cross-community bridge._
- **Are the 7 inferred relationships involving `AppMain_Run()` (e.g. with `cluster_app_run()` and `cluster_ui_get_page()`) actually correct?**
  _`AppMain_Run()` has 7 INFERRED edges - model-reasoned connections that need verification._
- **Should `System & Peripheral Init` be split into smaller, more focused modules?**
  _Cohesion score 0.13043478260869565 - nodes in this community are weakly interconnected._