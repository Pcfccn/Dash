# Graph Report - Dash  (2026-10-11)

## Corpus Check
- 63 files · ~779,773 words
- Verdict: corpus is large enough that graph structure adds value.
- Unclassified: 9 file(s) not represented in the graph (top: (none) 5, .ld 2, .ioc 1)

## Summary
- 811 nodes · 1694 edges · 40 communities (36 shown, 4 thin omitted)
- Extraction: 73% EXTRACTED · 27% INFERRED · 0% AMBIGUOUS · INFERRED: 464 edges (avg confidence: 0.88)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `c44ff048`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- In-vehicle install — from bench demo to a live car
- Чистовая сборка: из макетки в устройство
- main.c
- cluster_ui.c
- test_lv_port_disp.c
- Colorado 2.8 Diesel Cluster v2 Mockup
- fault.c
- fdcan_obd.c
- main.h
- SNIFF page (STATE + ANALOG)
- obd_rx_poll
- test_fdcan_obd.c
- stdint
- Architecture
- obd_is_fresh
- Сессия 2026-07-24, утро (~10:19–10:20) — расшифровка фото
- stm32h7xx_it.c
- obd_poll_tick
- FDCAN1_IT0_IRQHandler
- stm32h7xx_hal_conf.h
- Свой одометр по GPS и расход топлива
- build.py
- freertos.c
- tim.c
- REVIEW.md
- test_can_sniff.c
- CLAUDE.md
- GPS-скорость и замер разгона («свой Dragy»)
- Прогон 2026-07-21 (после ~5 мин поездки, снимок на парковке)
- SNIFF capture — selector / PRNDL search
- obd_init
- GM-enhanced DIDs (mode 22) for the Colorado 2.8 / E98
- Findings
- HAL_GPIO_Init
- Экран: качество картинки и скорость
- Oil pressure: where it comes from, and how to check it
- MX_SPI2_Init
- fault_record

## God Nodes (most connected - your core abstractions)
1. `obd_rx_poll()` - 88 edges
2. `cluster_ui_refresh()` - 40 edges
3. `obd_poll_tick()` - 35 edges
4. `Architecture` - 28 edges
5. `near()` - 25 edges
6. `lv_port_disp_service()` - 21 edges
7. `req_mode01()` - 20 edges
8. `disp_flush_cb()` - 19 edges
9. `Этап 0 дисплея (`b7f9470`) — что смотреть ревьюеру` - 19 edges
10. `AppMain_Run()` - 16 edges

## Surprising Connections (you probably didn't know these)
- `5. Known gaps for a "complete" in-car build` --references--> `backlight_set()`  [INFERRED]
  docs/vehicle-install.md → Core/Src/app_main.c
- `Подсветка: драйвер затвора` --references--> `AppMain_Init()`  [INFERRED]
  docs/future/build-hardware.md → Core/Src/app_main.c
- `Изменения в прошивке` --references--> `obd_rx_poll()`  [INFERRED]
  docs/future/gps-performance-meter.md → Core/Src/fdcan_obd.c
- `Ключевые соглашения (их легко сломать)` --references--> `obd_rx_poll()`  [INFERRED]
  docs/REVIEW.md → Core/Src/fdcan_obd.c
- `Поток управления` --references--> `obd_rx_poll()`  [INFERRED]
  docs/REVIEW.md → Core/Src/fdcan_obd.c

## Import Cycles
- None detected.

## Hyperedges (group relationships)
- **Three-Page Cluster Navigation (Drive/DPF/Diag)** — design_diesel_cluster_v2_3_drive_page, design_diesel_cluster_v2_3_dpf_page, design_diesel_cluster_v2_3_diag_page, design_diesel_cluster_v2_3_tab_switcher [EXTRACTED 1.00]
- **Glanceable Diesel-Health Signature Elements** — design_diesel_cluster_v2_3_systems_rail, design_diesel_cluster_v2_3_calibration_eyebrow, design_diesel_cluster_v2_3_state_colour_encoding, design_diesel_cluster_v2_3_ili9488_constraints [INFERRED 0.75]

## Communities (40 total, 4 thin omitted)

### Community 0 - "In-vehicle install — from bench demo to a live car"
Cohesion: 0.22
Nodes (9): 1. Powering the board in the car, 3. What the firmware sends, 4. Bring-up sequence, 5. Known gaps for a "complete" in-car build, Appendix: full connection & power map, In-vehicle install — from bench demo to a live car, Power distribution, Signal wiring (all MCU I/O is 3.3 V logic — do not exceed ~3.6 V on any pin) (+1 more)

### Community 1 - "Чистовая сборка: из макетки в устройство"
Cohesion: 0.06
Nodes (32): 1. Что не так с макетом, 2. Выбранная схема (вариант A) и что потом, 3. Схема узлов (вариант A), 4. Корпус и установка, 5. Порядок сборки и проверки, 6. Стоимость варианта A, 6a. Если бюджет позволяет (было в первой версии, ~$100–150), 6b. Заказ на LCSC (проверено по API LCSC 2026-10-11) (+24 more)

### Community 2 - "main.c"
Cohesion: 0.22
Nodes (11): FDCAN_HandleTypeDef, HAL_FDCAN_MspDeInit(), HAL_FDCAN_MspInit(), MX_FDCAN1_Init(), TIM_HandleTypeDef, Error_Handler(), HAL_TIM_PeriodElapsedCallback(), main() (+3 more)

### Community 3 - "cluster_ui.c"
Cohesion: 0.07
Nodes (66): Hardware notes worth knowing before touching related code, build_diag(), build_drive(), build_sniff(), build_strip(), lv_obj_t, metric_key_t, metric_state_t (+58 more)

### Community 4 - "test_lv_port_disp.c"
Cohesion: 0.05
Nodes (79): lv_area_t, lv_display_t, SPI_HandleTypeDef, cs_high(), cs_low(), dc_cmd(), dc_data(), disp_flush_cb() (+71 more)

### Community 5 - "Colorado 2.8 Diesel Cluster v2 Mockup"
Cohesion: 0.22
Nodes (13): Calibration Eyebrow, Colorado 2.8 Diesel Cluster v2 Mockup, Holden Colorado RG 2.8 E98 ECM, DIAG Page, DPF Page, DPF Soot / Regeneration Monitoring, DRIVE Page, ECU Instrument Direction (+5 more)

### Community 6 - "fault.c"
Cohesion: 0.25
Nodes (10): fault_init(), fault_reset_abnormal(), fault_reset_streak(), fault_reset_text(), fault_wdg_kick(), fault_wdg_run_mode(), fault_wdg_start_boot(), rec_set_streak() (+2 more)

### Community 7 - "fdcan_obd.c"
Cohesion: 0.10
Nodes (37): cluster_app_run(), obd_on_update(), arrived_in_time(), HAL_StatusTypeDef, can_send(), cfg_ok(), decode_mode01(), decode_mode22() (+29 more)

### Community 8 - "main.h"
Cohesion: 0.15
Nodes (4): HAL_StatusTypeDef, HAL_InitTick(), stm32h7xx_hal, stm32h7xx_hal_tim

### Community 9 - "SNIFF page (STATE + ANALOG)"
Cohesion: 0.10
Nodes (19): DIAG page, DPF page, DRIVE page, SNIFF page (STATE + ANALOG), Сессия 2026-07-23, утро (~10:10) — расшифровка фото, Фото 10 (628) — двигатель не запущен, Фото 11 (629), Фото 12 (630) — двигатель заглушен, WARN (+11 more)

### Community 10 - "obd_rx_poll"
Cohesion: 0.10
Nodes (37): obd_rx_poll(), req_mode01(), egt_request(), near(), t_cf_empty_rejected(), t_cf_other_sender_ignored(), t_cf_short_rejected(), t_cf_wrong_sequence() (+29 more)

### Community 11 - "test_fdcan_obd.c"
Cohesion: 0.14
Nodes (14): IRQn_Type, cap_scenario(), HAL_NVIC_DisableIRQ(), HAL_NVIC_EnableIRQ(), HAL_NVIC_SetPriority(), main(), push_frame(), reset() (+6 more)

### Community 12 - "stdint"
Cohesion: 0.15
Nodes (5): metric_key_t, metric_state_t, metric_state(), stdbool, stdint

### Community 13 - "Architecture"
Cohesion: 0.05
Nodes (66): can_sniff, Architecture, cluster_app, cluster_ui, metric_key_t, obd_data_t, policy_complete(), policy_ecm_fresh() (+58 more)

### Community 14 - "obd_is_fresh"
Cohesion: 0.20
Nodes (12): metric_key_t, obd_data_t, obd_is_fresh(), obd_mil_fresh(), obd_sel_fresh(), t_boost_needs_baro_and_follows_map(), t_decode_stamps_fresh(), t_delayed_frame_keeps_rx_time() (+4 more)

### Community 15 - "Сессия 2026-07-24, утро (~10:19–10:20) — расшифровка фото"
Cohesion: 0.11
Nodes (17): 1. OIL P нестабильна — приоритет высокий, 2. DPF данные нестабильны, 3. DPF page WARN при dP=NaN, 4. BATT 11.9V — WARN alert, DIAG page, DPF page, DRIVE page, SNIFF page (+9 more)

### Community 17 - "obd_poll_tick"
Cohesion: 0.11
Nodes (27): obd_poll_tick(), req_mode22(), req_oilp(), last_tx_is(), park(), t_d03_nrc78_within_cap_completes(), t_mil_goes_stale(), t_mil_invalid_after_three_misses() (+19 more)

### Community 18 - "FDCAN1_IT0_IRQHandler"
Cohesion: 0.27
Nodes (12): FDCAN1_IT0_IRQHandler(), obd_can_health(), obd_health_t, irq_mode(), t_d03_nrc78_total_cap(), t_d05_arrival_check_across_wrap(), t_d06_sniff_gets_rx_time(), t_delayed_frame_across_wrap() (+4 more)

### Community 19 - "stm32h7xx_hal_conf.h"
Cohesion: 0.03
Nodes (61): stm32h7xx_hal_adc, stm32h7xx_hal_cec, stm32h7xx_hal_comp, stm32h7xx_hal_cordic, stm32h7xx_hal_cortex, stm32h7xx_hal_crc, stm32h7xx_hal_cryp, stm32h7xx_hal_dac (+53 more)

### Community 21 - "Свой одометр по GPS и расход топлива"
Cohesion: 0.18
Nodes (11): 1. Что показываем, 2. Пробег: откуда брать расстояние, 3. Расход топлива: откуда брать, 4. Хранение, 5. Экран и управление, 6. Прошивка: из чего состоит, 7. Что сделать первым, Источники (+3 more)

### Community 22 - "build.py"
Cohesion: 0.15
Nodes (20): argparse, concurrent_futures, importlib_util, os, pathlib, re, shutil, subprocess (+12 more)

### Community 23 - "freertos.c"
Cohesion: 0.25
Nodes (4): app_main, MX_FREERTOS_Init(), main, task

### Community 24 - "tim.c"
Cohesion: 0.43
Nodes (6): TIM_HandleTypeDef, HAL_TIM_MspPostInit(), HAL_TIM_PWM_MspDeInit(), HAL_TIM_PWM_MspInit(), MX_TIM1_Init(), tim

### Community 27 - "test_can_sniff.c"
Cohesion: 0.10
Nodes (36): apply_filter(), can_sniff_candidates(), can_sniff_feed(), can_sniff_filter_error(), can_sniff_fps(), can_sniff_id_count(), can_sniff_is_active(), can_sniff_movers() (+28 more)

### Community 28 - "CLAUDE.md"
Cohesion: 0.29
Nodes (5): Build / flash / debug, Git workflow: commit and push automatically, graphify, Use graphify before and after non-trivial changes, What this is

### Community 29 - "GPS-скорость и замер разгона («свой Dragy»)"
Cohesion: 0.11
Nodes (18): 1. Зачем GPS, если есть OBD-скорость, 2. Аналоги, 3. Выбор GNSS-чипа, 4. Интеграция в текущую систему, 5. План прошивки и трудозатраты, 6. Подводные камни, 7. Резервное питание модуля: не нужно, 8. Железо: варианты и BOM (+10 more)

### Community 30 - "Прогон 2026-07-21 (после ~5 мин поездки, снимок на парковке)"
Cohesion: 0.20
Nodes (9): Анализ: что ответили новые enhanced-DID, Выводы / что дальше, КРИТИЧНО: скорость/обороты/rail/batt/load не читались, Прогон 2026-07-21 (после ~5 мин поездки, снимок на парковке), Расшифровка, Фото 1–4 — SNIFF (широковещательные кадры, мотор заглушён), Фото 5 — DIAG, Фото 6 — DPF (+1 more)

### Community 31 - "SNIFF capture — selector / PRNDL search"
Cohesion: 0.29
Nodes (6): Analysis — distinct value set per interesting byte, Conclusion, Next, Run 1  (design/Photos/1/, frames 023–035), Run 2  (design/Photos/2/, frames 036–052), SNIFF capture — selector / PRNDL search

### Community 32 - "obd_init"
Cohesion: 0.13
Nodes (26): FDCAN_HandleTypeDef, HAL_FDCAN_RxFifo0Callback(), obd_check_health(), obd_init(), rx_pump(), FDCAN_ProtocolStatusTypeDef, FDCAN_RxHeaderTypeDef, FDCAN_TxHeaderTypeDef (+18 more)

### Community 33 - "GM-enhanced DIDs (mode 22) for the Colorado 2.8 / E98"
Cohesion: 0.29
Nodes (7): Added, awaiting on-car confirmation, Being probed on the car (standard diesel PIDs), Confirmed on this vehicle, GM-enhanced DIDs (mode 22) for the Colorado 2.8 / E98, Oil pressure — mode 22 confirmed absent on this E98, Oil pressure via passive CAN broadcast (superseded), Sources

### Community 34 - "Findings"
Cohesion: 0.20
Nodes (9): 1. OIL P (oil pressure) — candidate 0x1BA[3] is NOT oil pressure — REJECT, 2. SPEED — ~10 s lag + ~12 % high, 3. OIL temp — plausible magnitude, trend to verify, 4. Reads OK (no action), DIAG page (photos 17–18), DRIVE page readings (photos 19–26), Findings, Road-test session — 2026-08-23 (26 photos) (+1 more)

### Community 35 - "HAL_GPIO_Init"
Cohesion: 0.33
Nodes (5): MX_GPIO_Init(), gpio, GPIO_InitTypeDef, GPIO_TypeDef, HAL_GPIO_Init()

### Community 36 - "Экран: качество картинки и скорость"
Cohesion: 0.14
Nodes (14): 2. Время передачи кадра, 3. Этап 0: без покупок (сделать в любом случае), 4. Варианты железа, 5. Рекомендация, 6. Что купить, 6a. Что из этого есть на LCSC (проверено 2026-10-09), 7. Подводные камни, 8. Вопросы до начала работы (+6 more)

### Community 37 - "Oil pressure: where it comes from, and how to check it"
Cohesion: 0.25
Nodes (6): Check on the car, DIAG, bottom readout (line 3), How the firmware asks (`fdcan_obd.c`, "oil pressure"), Oil pressure: where it comes from, and how to check it, The source, What the old candidates were

### Community 38 - "MX_SPI2_Init"
Cohesion: 0.40
Nodes (5): SPI_HandleTypeDef, HAL_SPI_MspDeInit(), HAL_SPI_MspInit(), MX_SPI2_Init(), spi

### Community 39 - "fault_record"
Cohesion: 0.40
Nodes (5): fault_record(), vApplicationStackOverflowHook(), HardFault_Handler(), fault_code_t, TaskHandle_t

## Knowledge Gaps
- **140 isolated node(s):** `What this is`, `Git workflow: commit and push automatically`, `Use graphify before and after non-trivial changes`, `graphify`, `Фото 7 (625) — двигатель работает, RPM=1056` (+135 more)
  These have ≤1 connection - possible missing edges or undocumented components. (Counts symbols only; 283 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **4 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `obd_rx_poll()` connect `obd_rx_poll` to `obd_init`, `cluster_ui.c`, `fdcan_obd.c`, `test_fdcan_obd.c`, `Architecture`, `obd_is_fresh`, `obd_poll_tick`, `FDCAN1_IT0_IRQHandler`, `test_can_sniff.c`, `GPS-скорость и замер разгона («свой Dragy»)`?**
  _High betweenness centrality (0.095) - this node is a cross-community bridge._
- **Why does `Architecture` connect `Architecture` to `obd_init`, `main.c`, `cluster_ui.c`, `test_lv_port_disp.c`, `fault.c`, `fdcan_obd.c`, `obd_rx_poll`, `obd_is_fresh`, `obd_poll_tick`, `FDCAN1_IT0_IRQHandler`, `test_can_sniff.c`, `CLAUDE.md`?**
  _High betweenness centrality (0.083) - this node is a cross-community bridge._
- **Why does `cluster_ui_refresh()` connect `cluster_ui.c` to `test_lv_port_disp.c`, `fdcan_obd.c`, `stdint`, `Architecture`, `obd_is_fresh`, `FDCAN1_IT0_IRQHandler`, `test_can_sniff.c`?**
  _High betweenness centrality (0.082) - this node is a cross-community bridge._
- **Are the 74 inferred relationships involving `obd_rx_poll()` (e.g. with `Architecture` and `cluster_app_run()`) actually correct?**
  _`obd_rx_poll()` has 74 INFERRED edges - model-reasoned connections that need verification._
- **Are the 27 inferred relationships involving `cluster_ui_refresh()` (e.g. with `Architecture` and `Hardware notes worth knowing before touching related code`) actually correct?**
  _`cluster_ui_refresh()` has 27 INFERRED edges - model-reasoned connections that need verification._
- **Are the 30 inferred relationships involving `obd_poll_tick()` (e.g. with `Architecture` and `cluster_app_run()`) actually correct?**
  _`obd_poll_tick()` has 30 INFERRED edges - model-reasoned connections that need verification._
- **Are the 27 inferred relationships involving `Architecture` (e.g. with `policy_complete()` and `policy_ecm_fresh()`) actually correct?**
  _`Architecture` has 27 INFERRED edges - model-reasoned connections that need verification._