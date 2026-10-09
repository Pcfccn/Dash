# Graph Report - Dash  (2026-10-09)

## Corpus Check
- 58 files · ~752,535 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 476 nodes · 783 edges · 36 communities (34 shown, 2 thin omitted)
- Extraction: 82% EXTRACTED · 18% INFERRED · 0% AMBIGUOUS · INFERRED: 140 edges (avg confidence: 0.8)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `b7f94702`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- Metric Model & App Glue
- App Main & Superloop
- UI Page Builders
- LVGL Display Port
- Cluster Design Spec
- UI Refresh & Metric Helpers
- Interrupt Handlers
- Peripheral Headers
- HAL Timebase
- Cluster App Header
- FDCAN OBD Header
- Metric Key Enum
- Metric State Enum
- set_metric
- SNIFF page (STATE + ANALOG)
- metric_state_t
- SNIFF capture — selector / PRNDL search
- Сессия 2026-07-24, утро (~10:19–10:20) — расшифровка фото
- Oil-pressure source test (throttle-blip correlation)
- Findings
- Экран: качество картинки и скорость

## God Nodes (most connected - your core abstractions)
1. `obd_rx_poll()` - 45 edges
2. `cluster_ui_refresh()` - 17 edges
3. `HAL_GetTick()` - 17 edges
4. `obd_poll_tick()` - 16 edges
5. `set_metric()` - 15 edges
6. `AppMain_Run()` - 13 edges
7. `req_mode01()` - 13 edges
8. `build_drive()` - 11 edges
9. `Dash — обзор проекта для ревью` - 11 edges
10. `GPS-скорость и замер разгона («свой Dragy»)` - 11 edges

## Surprising Connections (you probably didn't know these)
- `HAL_GPIO_EXTI_Callback()` --calls--> `HAL_GetTick()`  [INFERRED]
  Core/Src/app_main.c → tests/host/test_fdcan_obd.c
- `AppMain_Run()` --calls--> `HAL_GetTick()`  [INFERRED]
  Core/Src/app_main.c → tests/host/test_fdcan_obd.c
- `apply_filter()` --calls--> `HAL_FDCAN_ConfigFilter()`  [INFERRED]
  Core/Src/can_sniff.c → tests/host/test_fdcan_obd.c
- `can_sniff_top_ids()` --calls--> `HAL_GetTick()`  [INFERRED]
  Core/Src/can_sniff.c → tests/host/test_fdcan_obd.c
- `cluster_app_run()` --calls--> `HAL_GetTick()`  [INFERRED]
  Core/Src/cluster_app.c → tests/host/test_fdcan_obd.c

## Import Cycles
- None detected.

## Hyperedges (group relationships)
- **Three-Page Cluster Navigation (Drive/DPF/Diag)** — design_diesel_cluster_v2_3_drive_page, design_diesel_cluster_v2_3_dpf_page, design_diesel_cluster_v2_3_diag_page, design_diesel_cluster_v2_3_tab_switcher [EXTRACTED 1.00]
- **Glanceable Diesel-Health Signature Elements** — design_diesel_cluster_v2_3_systems_rail, design_diesel_cluster_v2_3_calibration_eyebrow, design_diesel_cluster_v2_3_state_colour_encoding, design_diesel_cluster_v2_3_ili9488_constraints [INFERRED 0.75]

## Communities (36 total, 2 thin omitted)

### Community 1 - "Metric Model & App Glue"
Cohesion: 0.18
Nodes (20): cluster_app_run(), obd_on_update(), can_send(), decode_mode01(), decode_mode22(), dispatch(), mil_attempt_failed(), mode01_len() (+12 more)

### Community 2 - "App Main & Superloop"
Cohesion: 0.06
Nodes (35): fault_init(), fault_record(), fault_reset_abnormal(), fault_reset_text(), fault_selftest(), fault_wdg_kick(), fault_wdg_run_mode(), fault_wdg_start_boot() (+27 more)

### Community 3 - "UI Page Builders"
Cohesion: 0.14
Nodes (39): metric_key_t, metric_state_t, metric_state(), build_diag(), build_drive(), build_sniff(), build_strip(), metric_key_t (+31 more)

### Community 4 - "LVGL Display Port"
Cohesion: 0.20
Nodes (18): lv_display_t, SPI_HandleTypeDef, cs_high(), cs_low(), dc_cmd(), dc_data(), disp_flush_cb(), dma_wait() (+10 more)

### Community 5 - "Cluster Design Spec"
Cohesion: 0.22
Nodes (13): Calibration Eyebrow, Colorado 2.8 Diesel Cluster v2 Mockup, Holden Colorado RG 2.8 E98 ECM, DIAG Page, DPF Page, DPF Soot / Regeneration Monitoring, DRIVE Page, ECU Instrument Direction (+5 more)

### Community 6 - "UI Refresh & Metric Helpers"
Cohesion: 0.07
Nodes (26): Возможные доработки, 1. Powering the board in the car, 2. Connecting to the CAN bus (OBD-II port), 3. Read-only safety, 4. Bring-up sequence, 5. Known gaps for a "complete" in-car build, Appendix: full connection & power map, ⚠️ Bus termination — the #1 thing to get right (+18 more)

### Community 7 - "Interrupt Handlers"
Cohesion: 0.10
Nodes (45): obd_poll_tick(), obd_rx_poll(), reply_is_ours(), req_mode01(), req_mode22(), egt_request(), main(), near() (+37 more)

### Community 9 - "HAL Timebase"
Cohesion: 0.10
Nodes (19): DIAG page, DPF page, DRIVE page, SNIFF page (STATE + ANALOG), Сессия 2026-07-23, утро (~10:10) — расшифровка фото, Фото 10 (628) — двигатель не запущен, Фото 11 (629), Фото 12 (630) — двигатель заглушен, WARN (+11 more)

### Community 13 - "Cluster App Header"
Cohesion: 0.10
Nodes (21): 1. Что это, 2. Что работает в машине (по заметкам с выездов), 3. Архитектура, 4. Сборка, прошивка, инструменты, 5. Находки ревью, 6. Что сделано хорошо, 7. Устаревшая документация (расходится с кодом), 8. Вопросы автору (+13 more)

### Community 15 - "FDCAN OBD Header"
Cohesion: 0.11
Nodes (17): 1. OIL P нестабильна — приоритет высокий, 2. DPF данные нестабильны, 3. DPF page WARN при dP=NaN, 4. BATT 11.9V — WARN alert, DIAG page, DPF page, DRIVE page, SNIFF page (+9 more)

### Community 22 - "Metric Key Enum"
Cohesion: 0.54
Nodes (7): compile_one(), find_toolchain(), main(), obj_for(), sources(), tool(), up_to_date()

### Community 23 - "Metric State Enum"
Cohesion: 0.15
Nodes (22): app_loop_max_ms(), AppMain_Init(), AppMain_Run(), backlight_set(), heartbeat_led_init(), heartbeat_led_tick(), key_button_init(), key_tap() (+14 more)

### Community 27 - "set_metric"
Cohesion: 0.15
Nodes (16): HAL_GPIO_EXTI_Callback(), apply_filter(), can_sniff_candidates(), can_sniff_feed(), can_sniff_fps(), can_sniff_id_count(), can_sniff_is_active(), can_sniff_movers() (+8 more)

### Community 29 - "SNIFF page (STATE + ANALOG)"
Cohesion: 0.11
Nodes (18): 1. Зачем GPS, если есть OBD-скорость, 2. Аналоги, 3. Выбор GNSS-чипа, 4. Интеграция в текущую систему, 5. План прошивки и трудозатраты, 6. Подводные камни, 7. Резервное питание модуля: не нужно, 8. Железо: варианты и BOM (+10 more)

### Community 30 - "metric_state_t"
Cohesion: 0.20
Nodes (9): Анализ: что ответили новые enhanced-DID, Выводы / что дальше, КРИТИЧНО: скорость/обороты/rail/batt/load не читались, Прогон 2026-07-21 (после ~5 мин поездки, снимок на парковке), Расшифровка, Фото 1–4 — SNIFF (широковещательные кадры, мотор заглушён), Фото 5 — DIAG, Фото 6 — DPF (+1 more)

### Community 31 - "SNIFF capture — selector / PRNDL search"
Cohesion: 0.06
Nodes (27): Architecture, Build / flash / debug, Git workflow: commit and push automatically, graphify, Hardware notes worth knowing before touching related code, Use graphify before and after non-trivial changes, What this is, Added, awaiting on-car confirmation (+19 more)

### Community 32 - "Сессия 2026-07-24, утро (~10:19–10:20) — расшифровка фото"
Cohesion: 0.16
Nodes (19): FDCAN_HandleTypeDef, cluster_app_init(), FDCAN_HandleTypeDef, obd_check_health(), obd_init(), FDCAN_FilterTypeDef, FDCAN_ProtocolStatusTypeDef, FDCAN_RxHeaderTypeDef (+11 more)

### Community 34 - "Findings"
Cohesion: 0.20
Nodes (9): 1. OIL P (oil pressure) — candidate 0x1BA[3] is NOT oil pressure — REJECT, 2. SPEED — ~10 s lag + ~12 % high, 3. OIL temp — plausible magnitude, trend to verify, 4. Reads OK (no action), DIAG page (photos 17–18), DRIVE page readings (photos 19–26), Findings, Road-test session — 2026-08-23 (26 photos) (+1 more)

### Community 36 - "Экран: качество картинки и скорость"
Cohesion: 0.13
Nodes (15): 1. Что сейчас и где теряется, 2. Время передачи кадра, 3. Этап 0: без покупок (сделать в любом случае), 4. Варианты железа, 5. Рекомендация, 6. Что купить, 6a. Что из этого есть на LCSC (проверено 2026-10-09), 7. Подводные камни (+7 more)

## Knowledge Gaps
- **128 isolated node(s):** `What this is`, `Git workflow: commit and push automatically`, `Build / flash / debug`, `Architecture`, `Use graphify before and after non-trivial changes` (+123 more)
  These have ≤1 connection - possible missing edges or undocumented components.
- **2 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `HAL_GetTick()` connect `set_metric` to `Сессия 2026-07-24, утро (~10:19–10:20) — расшифровка фото`, `Metric Model & App Glue`, `App Main & Superloop`, `LVGL Display Port`, `Interrupt Handlers`, `Metric State Enum`?**
  _High betweenness centrality (0.097) - this node is a cross-community bridge._
- **Why does `obd_rx_poll()` connect `Interrupt Handlers` to `Сессия 2026-07-24, утро (~10:19–10:20) — расшифровка фото`, `Metric Model & App Glue`, `set_metric`?**
  _High betweenness centrality (0.051) - this node is a cross-community bridge._
- **Why does `wdg_set()` connect `App Main & Superloop` to `set_metric`?**
  _High betweenness centrality (0.039) - this node is a cross-community bridge._
- **Are the 37 inferred relationships involving `obd_rx_poll()` (e.g. with `cluster_app_run()` and `can_sniff_feed()`) actually correct?**
  _`obd_rx_poll()` has 37 INFERRED edges - model-reasoned connections that need verification._
- **Are the 8 inferred relationships involving `cluster_ui_refresh()` (e.g. with `cluster_app_run()` and `metric_state()`) actually correct?**
  _`cluster_ui_refresh()` has 8 INFERRED edges - model-reasoned connections that need verification._
- **Are the 16 inferred relationships involving `HAL_GetTick()` (e.g. with `AppMain_Run()` and `HAL_GPIO_EXTI_Callback()`) actually correct?**
  _`HAL_GetTick()` has 16 INFERRED edges - model-reasoned connections that need verification._
- **Are the 11 inferred relationships involving `obd_poll_tick()` (e.g. with `cluster_app_run()` and `can_sniff_is_active()`) actually correct?**
  _`obd_poll_tick()` has 11 INFERRED edges - model-reasoned connections that need verification._