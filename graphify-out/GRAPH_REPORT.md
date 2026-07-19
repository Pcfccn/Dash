# Graph Report - C:\Users\Roma\Documents\stm\Dash  (2026-07-18)

## Corpus Check
- 36 files · ~0 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 158 nodes · 275 edges · 24 communities (23 shown, 1 thin omitted)
- Extraction: 87% EXTRACTED · 13% INFERRED · 0% AMBIGUOUS · INFERRED: 35 edges (avg confidence: 0.8)
- Token cost: 0 input · 0 output

## Community Hubs (Navigation)
- CAN & board init
- OBD-II app glue
- Cluster UI pages
- Metrics & formatting
- Main loop / backlight & KEY
- ILI9488 display driver
- ST7735 status screen
- Project docs (CLAUDE.md)
- HAL timebase

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
- `main()` --calls--> `MX_FREERTOS_Init()`  [INFERRED]
  Core/Src/main.c → Core/Src/freertos.c
- `AppMain_Init()` --calls--> `cluster_app_init()`  [INFERRED]
  Core/Src/app_main.c → Core/Src/cluster_app.c
- `AppMain_Init()` --calls--> `lv_port_disp_init()`  [INFERRED]
  Core/Src/app_main.c → Core/Src/lv_port_disp.c
- `AppMain_Init()` --calls--> `st7735_status_init()`  [INFERRED]
  Core/Src/app_main.c → Core/Src/st7735_status.c
- `AppMain_Run()` --calls--> `cluster_app_run()`  [INFERRED]
  Core/Src/app_main.c → Core/Src/cluster_app.c

## Import Cycles
- None detected.

## Communities (24 total, 1 thin omitted)

### Community 0 - "CAN & board init"
Cohesion: 0.12
Nodes (20): FDCAN_HandleTypeDef, HAL_FDCAN_MspDeInit(), HAL_FDCAN_MspInit(), MX_FDCAN1_Init(), MX_GPIO_Init(), TIM_HandleTypeDef, Error_Handler(), HAL_TIM_PeriodElapsedCallback() (+12 more)

### Community 1 - "OBD-II app glue"
Cohesion: 0.19
Nodes (17): FDCAN_HandleTypeDef, cluster_app_init(), cluster_app_run(), obd_on_update(), FDCAN_HandleTypeDef, can_send(), decode_mode01(), dispatch() (+9 more)

### Community 2 - "Cluster UI pages"
Cohesion: 0.32
Nodes (18): build_diag(), build_dpf(), build_drive(), build_strip(), cluster_ui_build(), cluster_ui_next_page(), cluster_ui_set_page(), mk_bar() (+10 more)

### Community 3 - "Metrics & formatting"
Cohesion: 0.24
Nodes (15): metric_key_t, metric_state_t, metric_state(), metric_key_t, metric_state_t, cluster_ui_refresh(), fmt(), iround() (+7 more)

### Community 4 - "Main loop / backlight & KEY"
Cohesion: 0.22
Nodes (12): AppMain_Init(), AppMain_Run(), backlight_set(), heartbeat_led_init(), heartbeat_led_tick(), key_button_init(), cluster_ui_get_page(), MX_FREERTOS_Init() (+4 more)

### Community 5 - "ILI9488 display driver"
Cohesion: 0.33
Nodes (13): lv_display_t, cs_high(), cs_low(), dc_cmd(), dc_data(), disp_flush_cb(), ili9488_init_sequence(), lcd_set_window() (+5 more)

### Community 6 - "ST7735 status screen"
Cohesion: 0.44
Nodes (9): lv_display_t, st7735_status_init(), st_cs_high(), st_cs_low(), st_dc_cmd(), st_dc_data(), st_hw_init(), st_send_cmd() (+1 more)

### Community 8 - "Project docs (CLAUDE.md)"
Cohesion: 0.29
Nodes (6): Architecture (the parts we own, in `Core/`), Build / conventions, Dash — STM32H743 diesel instrument cluster, Design language (tuned for the weak ILI9488 panel), Hardware (WeAct MiniSTM32H743VIT6), Repo

## Knowledge Gaps
- **5 isolated node(s):** `Hardware (WeAct MiniSTM32H743VIT6)`, `Build / conventions`, `Architecture (the parts we own, in `Core/`)`, `Design language (tuned for the weak ILI9488 panel)`, `Repo`
  These have ≤1 connection - possible missing edges or undocumented components.
- **1 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `AppMain_Init()` connect `Main loop / backlight & KEY` to `OBD-II app glue`, `ILI9488 display driver`, `ST7735 status screen`?**
  _High betweenness centrality (0.184) - this node is a cross-community bridge._
- **Why does `AppMain_Run()` connect `Main loop / backlight & KEY` to `OBD-II app glue`, `Cluster UI pages`?**
  _High betweenness centrality (0.177) - this node is a cross-community bridge._
- **Why does `Error_Handler()` connect `CAN & board init` to `ST7735 status screen`?**
  _High betweenness centrality (0.130) - this node is a cross-community bridge._
- **Are the 7 inferred relationships involving `AppMain_Run()` (e.g. with `cluster_app_run()` and `cluster_ui_get_page()`) actually correct?**
  _`AppMain_Run()` has 7 INFERRED edges - model-reasoned connections that need verification._
- **What connects `Hardware (WeAct MiniSTM32H743VIT6)`, `Build / conventions`, `Architecture (the parts we own, in `Core/`)` to the rest of the system?**
  _5 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `CAN & board init` be split into smaller, more focused modules?**
  _Cohesion score 0.11692307692307692 - nodes in this community are weakly interconnected._