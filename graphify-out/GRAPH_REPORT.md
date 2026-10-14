# Graph Report - Dash  (2026-10-14)

## Corpus Check
- 75 files · ~797,217 words
- Verdict: corpus is large enough that graph structure adds value.
- Unclassified: 9 file(s) not represented in the graph (top: (none) 5, .ld 2, .ioc 1)

## Summary
- 936 nodes · 1994 edges · 47 communities (44 shown, 3 thin omitted)
- Extraction: 74% EXTRACTED · 26% INFERRED · 0% AMBIGUOUS · INFERRED: 528 edges (avg confidence: 0.87)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `8f17e54c`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- lv_port_disp.c
- Чистовая сборка: из макетки в устройство
- HAL_GPIO_Init
- cluster_ui.c
- test_lv_port_disp.c
- Colorado 2.8 Diesel Cluster v2 Mockup
- fault.c
- fdcan_obd.c
- stm32h7xx_hal_timebase_tim.c
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
- test_gps.c
- test_can_sniff.c
- test_input.c
- GPS-скорость и замер разгона («свой Dragy»)
- Прогон 2026-07-21 (после ~5 мин поездки, снимок на парковке)
- st7735_status.c
- obd_init
- Этап 0 дисплея (`b7f9470`) — что смотреть ревьюеру
- Findings
- main.c
- Экран: качество картинки и скорость
- lv_port_disp_init
- REVIEW.md
- Плата-носитель 7×9 см: раскладка и провода
- In-vehicle install — from bench demo to a live car
- Статус исправлений (2026-10-09)
- Oil pressure: where it comes from, and how to check it
- GM-enhanced DIDs (mode 22) for the Colorado 2.8 / E98
- SNIFF capture — selector / PRNDL search
- lvgl.h
- 8. Железо: варианты и BOM

## God Nodes (most connected - your core abstractions)
1. `obd_rx_poll()` - 90 edges
2. `cluster_ui_refresh()` - 45 edges
3. `obd_poll_tick()` - 37 edges
4. `Architecture` - 33 edges
5. `near()` - 25 edges
6. `AppMain_Run()` - 23 edges
7. `req_mode01()` - 21 edges
8. `lv_port_disp_service()` - 21 edges
9. `disp_flush_cb()` - 19 edges
10. `Этап 0 дисплея (`b7f9470`) — что смотреть ревьюеру` - 19 edges

## Surprising Connections (you probably didn't know these)
- `5. Known gaps for a "complete" in-car build` --references--> `backlight_set()`  [INFERRED]
  docs/vehicle-install.md → Core/Src/app_main.c
- `Подсветка: драйвер затвора` --references--> `AppMain_Init()`  [INFERRED]
  docs/future/build-hardware.md → Core/Src/app_main.c
- `Канонический аудит Stage C (2026-10-10) — статус по пунктам` --references--> `cluster_ui_set_page()`  [INFERRED]
  docs/REVIEW.md → Core/Src/cluster_ui.c
- `Ключевые соглашения (их легко сломать)` --references--> `obd_rx_poll()`  [INFERRED]
  docs/REVIEW.md → Core/Src/fdcan_obd.c
- `Поток управления` --references--> `obd_rx_poll()`  [INFERRED]
  docs/REVIEW.md → Core/Src/fdcan_obd.c

## Import Cycles
- None detected.

## Hyperedges (group relationships)
- **Three-Page Cluster Navigation (Drive/DPF/Diag)** — design_diesel_cluster_v2_3_drive_page, design_diesel_cluster_v2_3_dpf_page, design_diesel_cluster_v2_3_diag_page, design_diesel_cluster_v2_3_tab_switcher [EXTRACTED 1.00]
- **Glanceable Diesel-Health Signature Elements** — design_diesel_cluster_v2_3_systems_rail, design_diesel_cluster_v2_3_calibration_eyebrow, design_diesel_cluster_v2_3_state_colour_encoding, design_diesel_cluster_v2_3_ili9488_constraints [INFERRED 0.75]

## Communities (47 total, 3 thin omitted)

### Community 0 - "lv_port_disp.c"
Cohesion: 0.25
Nodes (19): lv_area_t, lv_display_t, cs_high(), cs_low(), dc_cmd(), dc_data(), disp_flush_cb(), dma_wait() (+11 more)

### Community 1 - "Чистовая сборка: из макетки в устройство"
Cohesion: 0.06
Nodes (33): 1. Что не так с макетом, 2. Выбранная схема (вариант A) и что потом, 3. Схема узлов (вариант A), 4. Корпус и установка, 5. Порядок сборки и проверки, 6. Стоимость варианта A, 6a. Если бюджет позволяет (было в первой версии, ~$100–150), 6b. Заказ на LCSC (проверено по API LCSC 2026-10-11) (+25 more)

### Community 2 - "HAL_GPIO_Init"
Cohesion: 0.18
Nodes (13): FDCAN_HandleTypeDef, HAL_FDCAN_MspDeInit(), HAL_FDCAN_MspInit(), MX_FDCAN1_Init(), Error_Handler(), SPI_HandleTypeDef, HAL_SPI_MspDeInit(), HAL_SPI_MspInit() (+5 more)

### Community 3 - "cluster_ui.c"
Cohesion: 0.07
Nodes (63): app_loop_max_ms(), build_diag(), build_drive(), build_sniff(), build_strip(), lv_obj_t, metric_key_t, metric_state_t (+55 more)

### Community 4 - "test_lv_port_disp.c"
Cohesion: 0.14
Nodes (27): lv_port_disp_service(), IRQn_Type, lv_obj_t, dma_done(), flush(), HAL_NVIC_EnableIRQ(), HAL_NVIC_SetPriority(), lv_display_get_screen_active() (+19 more)

### Community 5 - "Colorado 2.8 Diesel Cluster v2 Mockup"
Cohesion: 0.22
Nodes (13): Calibration Eyebrow, Colorado 2.8 Diesel Cluster v2 Mockup, Holden Colorado RG 2.8 E98 ECM, DIAG Page, DPF Page, DPF Soot / Regeneration Monitoring, DRIVE Page, ECU Instrument Direction (+5 more)

### Community 6 - "fault.c"
Cohesion: 0.23
Nodes (11): fault_init(), fault_reset_abnormal(), fault_reset_streak(), fault_reset_text(), fault_selftest(), fault_wdg_kick(), fault_wdg_run_mode(), fault_wdg_start_boot() (+3 more)

### Community 7 - "fdcan_obd.c"
Cohesion: 0.08
Nodes (47): cluster_app_run(), obd_on_update(), HAL_StatusTypeDef, can_send(), cfg_ok(), decode_mode01(), decode_mode22(), dispatch() (+39 more)

### Community 8 - "stm32h7xx_hal_timebase_tim.c"
Cohesion: 0.33
Nodes (3): HAL_StatusTypeDef, HAL_InitTick(), stm32h7xx_hal_tim

### Community 9 - "SNIFF page (STATE + ANALOG)"
Cohesion: 0.10
Nodes (19): DIAG page, DPF page, DRIVE page, SNIFF page (STATE + ANALOG), Сессия 2026-07-23, утро (~10:10) — расшифровка фото, Фото 10 (628) — двигатель не запущен, Фото 11 (629), Фото 12 (630) — двигатель заглушен, WARN (+11 more)

### Community 10 - "obd_rx_poll"
Cohesion: 0.09
Nodes (40): can_sniff_feed(), arrived_in_time(), obd_rx_poll(), req_mode01(), egt_request(), near(), t_cf_empty_rejected(), t_cf_other_sender_ignored() (+32 more)

### Community 11 - "test_fdcan_obd.c"
Cohesion: 0.12
Nodes (18): IRQn_Type, metric_key_t, cap_scenario(), HAL_NVIC_DisableIRQ(), HAL_NVIC_EnableIRQ(), HAL_NVIC_SetPriority(), main(), push_frame() (+10 more)

### Community 12 - "stdint"
Cohesion: 0.10
Nodes (6): metric_key_t, metric_state_t, metric_state(), stdbool, stdint, stm32h7xx_hal

### Community 13 - "Architecture"
Cohesion: 0.06
Nodes (56): can_sniff, Architecture, Build / flash / debug, Git workflow: commit and push automatically, graphify, Hardware notes worth knowing before touching related code, Use graphify before and after non-trivial changes, What this is (+48 more)

### Community 14 - "obd_is_fresh"
Cohesion: 0.21
Nodes (12): metric_key_t, obd_data_t, obd_is_fresh(), obd_sel_fresh(), oilp_standstill(), t_boost_needs_baro_and_follows_map(), t_decode_stamps_fresh(), t_delayed_frame_across_wrap() (+4 more)

### Community 15 - "Сессия 2026-07-24, утро (~10:19–10:20) — расшифровка фото"
Cohesion: 0.11
Nodes (17): 1. OIL P нестабильна — приоритет высокий, 2. DPF данные нестабильны, 3. DPF page WARN при dP=NaN, 4. BATT 11.9V — WARN alert, DIAG page, DPF page, DRIVE page, SNIFF page (+9 more)

### Community 16 - "stm32h7xx_it.c"
Cohesion: 0.14
Nodes (7): fault_record(), vApplicationStackOverflowHook(), HardFault_Handler(), fault, fault_code_t, stm32h7xx_it, TaskHandle_t

### Community 17 - "obd_poll_tick"
Cohesion: 0.11
Nodes (27): can_sniff_is_active(), obd_poll_tick(), req_mode22(), req_oilp(), last_tx_is(), park(), t_d03_nrc78_total_cap(), t_d03_nrc78_within_cap_completes() (+19 more)

### Community 18 - "FDCAN1_IT0_IRQHandler"
Cohesion: 0.31
Nodes (11): FDCAN1_IT0_IRQHandler(), obd_can_health(), obd_health_t, irq_mode(), t_d05_arrival_check_across_wrap(), t_d05_late_arrival_is_not_an_answer(), t_d06_sniff_gets_rx_time(), t_health_check_masks_rx_irq() (+3 more)

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
Cohesion: 0.53
Nodes (5): TIM_HandleTypeDef, HAL_TIM_MspPostInit(), HAL_TIM_PWM_MspDeInit(), HAL_TIM_PWM_MspInit(), MX_TIM1_Init()

### Community 25 - "test_gps.c"
Cohesion: 0.06
Nodes (61): enter(), gps_fix_fresh(), gps_init(), gps_poll(), gps_state(), gps_hw_getc(), gps_hw_init(), gps_hw_now_ms() (+53 more)

### Community 27 - "test_can_sniff.c"
Cohesion: 0.11
Nodes (34): apply_filter(), can_sniff_candidates(), can_sniff_filter_error(), can_sniff_fps(), can_sniff_id_count(), can_sniff_movers(), can_sniff_reset(), can_sniff_restore_pending() (+26 more)

### Community 28 - "test_input.c"
Cohesion: 0.22
Nodes (18): btn_out_t, btn_t, btn_step(), daynight_step(), daynight_t, input, exti_press(), hold_level() (+10 more)

### Community 29 - "GPS-скорость и замер разгона («свой Dragy»)"
Cohesion: 0.15
Nodes (13): 1. Зачем GPS, если есть OBD-скорость, 2. Аналоги, 3. Выбор GNSS-чипа, 4. Интеграция в текущую систему, 5. План прошивки и трудозатраты, 6. Подводные камни, 7. Резервное питание модуля: не нужно, 9. Что решить перед началом (+5 more)

### Community 30 - "Прогон 2026-07-21 (после ~5 мин поездки, снимок на парковке)"
Cohesion: 0.20
Nodes (9): Анализ: что ответили новые enhanced-DID, Выводы / что дальше, КРИТИЧНО: скорость/обороты/rail/batt/load не читались, Прогон 2026-07-21 (после ~5 мин поездки, снимок на парковке), Расшифровка, Фото 1–4 — SNIFF (широковещательные кадры, мотор заглушён), Фото 5 — DIAG, Фото 6 — DPF (+1 more)

### Community 31 - "st7735_status.c"
Cohesion: 0.24
Nodes (16): lv_display_t, st7735_status_backlight(), st7735_status_init(), st7735_status_key_dbg(), st_cs_high(), st_cs_low(), st_dc_cmd(), st_dc_data() (+8 more)

### Community 32 - "obd_init"
Cohesion: 0.13
Nodes (26): FDCAN_HandleTypeDef, HAL_FDCAN_RxFifo0Callback(), obd_check_health(), obd_init(), rx_pump(), FDCAN_ProtocolStatusTypeDef, FDCAN_RxHeaderTypeDef, FDCAN_TxHeaderTypeDef (+18 more)

### Community 33 - "Этап 0 дисплея (`b7f9470`) — что смотреть ревьюеру"
Cohesion: 0.20
Nodes (16): DMA1_Stream0_IRQHandler(), spi2_dma_init(), SPI2_IRQHandler(), DMA_HandleTypeDef, Этап 0 дисплея (`b7f9470`) — что смотреть ревьюеру, HAL_StatusTypeDef, SPI_HandleTypeDef, HAL_DMA_Init() (+8 more)

### Community 34 - "Findings"
Cohesion: 0.20
Nodes (9): 1. OIL P (oil pressure) — candidate 0x1BA[3] is NOT oil pressure — REJECT, 2. SPEED — ~10 s lag + ~12 % high, 3. OIL temp — plausible magnitude, trend to verify, 4. Reads OK (no action), DIAG page (photos 17–18), DRIVE page readings (photos 19–26), Findings, Road-test session — 2026-08-23 (26 photos) (+1 more)

### Community 35 - "main.c"
Cohesion: 0.21
Nodes (9): MX_GPIO_Init(), TIM_HandleTypeDef, HAL_TIM_PeriodElapsedCallback(), main(), MPU_Config(), SystemClock_Config(), gpio, spi (+1 more)

### Community 36 - "Экран: качество картинки и скорость"
Cohesion: 0.13
Nodes (15): 1. Что сейчас и где теряется, 2. Время передачи кадра, 3. Этап 0: без покупок (сделать в любом случае), 4. Варианты железа, 5. Рекомендация, 6. Что купить, 6a. Что из этого есть на LCSC (проверено 2026-10-09), 7. Подводные камни (+7 more)

### Community 37 - "lv_port_disp_init"
Cohesion: 0.27
Nodes (10): lv_port_disp_init(), lv_display_flush_cb_t, lv_tick_get_cb_t, lv_display_t, lv_display_create(), lv_display_set_buffers(), lv_display_set_color_format(), lv_display_set_flush_cb() (+2 more)

### Community 39 - "Плата-носитель 7×9 см: раскладка и провода"
Cohesion: 0.22
Nodes (8): 1. Распиновка гребёнок WeAct, 2. Гнёзда под WeAct: хватит двух 1×40, 3. Клеммники 3.81 мм на сетке 2.54, 4. Что где стоит, 5. Провода, 6. Проверки до установки WeAct, 7. Высота в коробке 140×82×38, Плата-носитель 7×9 см: раскладка и провода

### Community 40 - "In-vehicle install — from bench demo to a live car"
Cohesion: 0.22
Nodes (9): 1. Powering the board in the car, 3. What the firmware sends, 4. Bring-up sequence, 5. Known gaps for a "complete" in-car build, Appendix: full connection & power map, In-vehicle install — from bench demo to a live car, Power distribution, Signal wiring (all MCU I/O is 3.3 V logic — do not exceed ~3.6 V on any pin) (+1 more)

### Community 41 - "Статус исправлений (2026-10-09)"
Cohesion: 0.25
Nodes (8): SPI_HandleTypeDef, HAL_SPI_TxCpltCallback(), Канонический аудит Stage C (2026-10-10) — статус по пунктам, Полный аудит 2026-10-10 — статус по пунктам, Ревью Stage C4 (2026-10-11) — статус, Ревью Stage C6 (2026-10-11) — статус, Статус исправлений (2026-10-09), Таблица пунктов

### Community 42 - "Oil pressure: where it comes from, and how to check it"
Cohesion: 0.25
Nodes (6): Check on the car, DIAG, bottom readout (line 3), How the firmware asks (`fdcan_obd.c`, "oil pressure"), Oil pressure: where it comes from, and how to check it, The source, What the old candidates were

### Community 43 - "GM-enhanced DIDs (mode 22) for the Colorado 2.8 / E98"
Cohesion: 0.29
Nodes (7): Added, awaiting on-car confirmation, Being probed on the car (standard diesel PIDs), Confirmed on this vehicle, GM-enhanced DIDs (mode 22) for the Colorado 2.8 / E98, Oil pressure — mode 22 confirmed absent on this E98, Oil pressure via passive CAN broadcast (superseded), Sources

### Community 44 - "SNIFF capture — selector / PRNDL search"
Cohesion: 0.29
Nodes (6): Analysis — distinct value set per interesting byte, Conclusion, Next, Run 1  (design/Photos/1/, frames 023–035), Run 2  (design/Photos/2/, frames 036–052), SNIFF capture — selector / PRNDL search

### Community 45 - "lvgl.h"
Cohesion: 0.50
Nodes (3): lv_area_t, lv_area_get_height(), lv_area_get_width()

### Community 46 - "8. Железо: варианты и BOM"
Cohesion: 0.40
Nodes (5): 8. Железо: варианты и BOM, Вариант A — готовый модуль на M10 (рекомендуется для прототипа), ~$30–60, Вариант B — 25 Гц на 4 системах, антенна на крыше, ~$95–110, Вариант C — своя плата на компонентах с LCSC, ~$13 за детали, Опционально (для любого варианта)

## Knowledge Gaps
- **150 isolated node(s):** `What this is`, `Git workflow: commit and push automatically`, `Use graphify before and after non-trivial changes`, `graphify`, `Фото 7 (625) — двигатель работает, RPM=1056` (+145 more)
  These have ≤1 connection - possible missing edges or undocumented components. (Counts symbols only; 307 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **3 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `Architecture` connect `Architecture` to `obd_init`, `cluster_ui.c`, `main.c`, `lv_port_disp_init`, `fault.c`, `fdcan_obd.c`, `obd_rx_poll`, `obd_is_fresh`, `obd_poll_tick`, `FDCAN1_IT0_IRQHandler`, `test_gps.c`, `test_can_sniff.c`, `test_input.c`, `st7735_status.c`?**
  _High betweenness centrality (0.088) - this node is a cross-community bridge._
- **Why does `cluster_ui_refresh()` connect `cluster_ui.c` to `Этап 0 дисплея (`b7f9470`) — что смотреть ревьюеру`, `Экран: качество картинки и скорость`, `fdcan_obd.c`, `stdint`, `Architecture`, `obd_is_fresh`, `obd_poll_tick`, `FDCAN1_IT0_IRQHandler`, `test_gps.c`, `test_can_sniff.c`?**
  _High betweenness centrality (0.083) - this node is a cross-community bridge._
- **Why does `obd_rx_poll()` connect `obd_rx_poll` to `obd_init`, `cluster_ui.c`, `fdcan_obd.c`, `test_fdcan_obd.c`, `Architecture`, `obd_is_fresh`, `obd_poll_tick`, `FDCAN1_IT0_IRQHandler`, `test_can_sniff.c`, `GPS-скорость и замер разгона («свой Dragy»)`?**
  _High betweenness centrality (0.076) - this node is a cross-community bridge._
- **Are the 76 inferred relationships involving `obd_rx_poll()` (e.g. with `Architecture` and `cluster_app_run()`) actually correct?**
  _`obd_rx_poll()` has 76 INFERRED edges - model-reasoned connections that need verification._
- **Are the 31 inferred relationships involving `cluster_ui_refresh()` (e.g. with `Architecture` and `Hardware notes worth knowing before touching related code`) actually correct?**
  _`cluster_ui_refresh()` has 31 INFERRED edges - model-reasoned connections that need verification._
- **Are the 31 inferred relationships involving `obd_poll_tick()` (e.g. with `Architecture` and `cluster_app_run()`) actually correct?**
  _`obd_poll_tick()` has 31 INFERRED edges - model-reasoned connections that need verification._
- **Are the 32 inferred relationships involving `Architecture` (e.g. with `policy_complete()` and `policy_ecm_fresh()`) actually correct?**
  _`Architecture` has 32 INFERRED edges - model-reasoned connections that need verification._