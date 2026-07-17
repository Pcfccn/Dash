# Graph Report - Dash  (2026-07-17)

## Corpus Check
- 35 files · ~17,747 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 158 nodes · 281 edges · 25 communities (24 shown, 1 thin omitted)
- Extraction: 88% EXTRACTED · 12% INFERRED · 0% AMBIGUOUS · INFERRED: 34 edges (avg confidence: 0.8)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `4a082a77`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- Error_Handler
- cluster_ui.c
- fdcan_obd.c
- set_metric
- st7735_status.c
- lv_port_disp.c
- AppMain_Init
- Dash — STM32H743 diesel instrument cluster
- tim.c
- stm32h7xx_hal_timebase_tim.c

## God Nodes (most connected - your core abstractions)
1. `build_drive()` - 12 edges
2. `set_metric()` - 12 edges
3. `mk_label()` - 10 edges
4. `cluster_ui_refresh()` - 10 edges
5. `build_dpf()` - 9 edges
6. `main()` - 8 edges
7. `Error_Handler()` - 8 edges
8. `AppMain_Init()` - 7 edges
9. `AppMain_Run()` - 7 edges
10. `cluster_app_run()` - 7 edges

## Surprising Connections (you probably didn't know these)
- `AppMain_Init()` --calls--> `lv_port_disp_init()`  [INFERRED]
  Core/Src/app_main.c → Core/Src/lv_port_disp.c
- `AppMain_Init()` --calls--> `st7735_status_init()`  [INFERRED]
  Core/Src/app_main.c → Core/Src/st7735_status.c
- `StartDefaultTask()` --calls--> `AppMain_Init()`  [INFERRED]
  Core/Src/freertos.c → Core/Src/app_main.c
- `AppMain_Run()` --calls--> `cluster_app_run()`  [INFERRED]
  Core/Src/app_main.c → Core/Src/cluster_app.c
- `AppMain_Run()` --calls--> `cluster_ui_get_page()`  [INFERRED]
  Core/Src/app_main.c → Core/Src/cluster_ui.c

## Import Cycles
- None detected.

## Communities (25 total, 1 thin omitted)

### Community 0 - "Error_Handler"
Cohesion: 0.13
Nodes (17): FDCAN_HandleTypeDef, HAL_FDCAN_MspDeInit(), HAL_FDCAN_MspInit(), MX_FDCAN1_Init(), MX_FREERTOS_Init(), StartDefaultTask(), MX_GPIO_Init(), TIM_HandleTypeDef (+9 more)

### Community 1 - "cluster_ui.c"
Cohesion: 0.28
Nodes (21): build_diag(), build_dpf(), build_drive(), build_strip(), chip_set(), cluster_ui_build(), cluster_ui_next_page(), cluster_ui_set_page() (+13 more)

### Community 2 - "fdcan_obd.c"
Cohesion: 0.26
Nodes (13): cluster_app_run(), obd_on_update(), can_send(), decode_mode01(), dispatch(), obd_demo_tick(), obd_poll_tick(), obd_rx_poll() (+5 more)

### Community 3 - "set_metric"
Cohesion: 0.24
Nodes (15): metric_key_t, metric_state_t, metric_state(), metric_key_t, metric_state_t, cluster_ui_refresh(), fmt(), iround() (+7 more)

### Community 4 - "st7735_status.c"
Cohesion: 0.27
Nodes (13): AppMain_Run(), cluster_ui_get_page(), lv_display_t, st7735_status_init(), st7735_status_key_dbg(), st7735_status_set(), st_cs_high(), st_cs_low() (+5 more)

### Community 5 - "lv_port_disp.c"
Cohesion: 0.33
Nodes (13): lv_display_t, cs_high(), cs_low(), dc_cmd(), dc_data(), disp_flush_cb(), ili9488_init_sequence(), lcd_set_window() (+5 more)

### Community 6 - "AppMain_Init"
Cohesion: 0.32
Nodes (7): AppMain_Init(), heartbeat_led_init(), key_button_init(), FDCAN_HandleTypeDef, cluster_app_init(), FDCAN_HandleTypeDef, obd_init()

### Community 8 - "Dash — STM32H743 diesel instrument cluster"
Cohesion: 0.29
Nodes (6): Architecture (the parts we own, in `Core/`), Build / conventions, Dash — STM32H743 diesel instrument cluster, Design language (tuned for the weak ILI9488 panel), Hardware (WeAct MiniSTM32H743VIT6), Repo

### Community 9 - "tim.c"
Cohesion: 0.53
Nodes (5): TIM_HandleTypeDef, HAL_TIM_MspPostInit(), HAL_TIM_PWM_MspDeInit(), HAL_TIM_PWM_MspInit(), MX_TIM1_Init()

## Knowledge Gaps
- **5 isolated node(s):** `Hardware (WeAct MiniSTM32H743VIT6)`, `Build / conventions`, `Architecture (the parts we own, in `Core/`)`, `Design language (tuned for the weak ILI9488 panel)`, `Repo`
  These have ≤1 connection - possible missing edges or undocumented components.
- **1 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `AppMain_Init()` connect `AppMain_Init` to `Error_Handler`, `st7735_status.c`, `lv_port_disp.c`?**
  _High betweenness centrality (0.183) - this node is a cross-community bridge._
- **Why does `AppMain_Run()` connect `st7735_status.c` to `Error_Handler`, `cluster_ui.c`, `fdcan_obd.c`, `AppMain_Init`?**
  _High betweenness centrality (0.166) - this node is a cross-community bridge._
- **Why does `Error_Handler()` connect `Error_Handler` to `tim.c`, `st7735_status.c`?**
  _High betweenness centrality (0.127) - this node is a cross-community bridge._
- **Are the 2 inferred relationships involving `cluster_ui_refresh()` (e.g. with `cluster_app_run()` and `metric_state()`) actually correct?**
  _`cluster_ui_refresh()` has 2 INFERRED edges - model-reasoned connections that need verification._
- **What connects `Hardware (WeAct MiniSTM32H743VIT6)`, `Build / conventions`, `Architecture (the parts we own, in `Core/`)` to the rest of the system?**
  _5 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `Error_Handler` be split into smaller, more focused modules?**
  _Cohesion score 0.13043478260869565 - nodes in this community are weakly interconnected._