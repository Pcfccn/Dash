# Graph Report - Dash  (2026-10-22)

## Corpus Check
- 110 files · ~851,221 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 1382 nodes · 2941 edges · 69 communities (57 shown, 12 thin omitted)
- Extraction: 79% EXTRACTED · 21% INFERRED · 0% AMBIGUOUS · INFERRED: 617 edges (avg confidence: 0.8)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `c89e3229`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- obd_rx_poll
- Чистовая сборка: из макетки в устройство
- Error_Handler
- cluster_ui.c
- test_lv_port_disp.c
- Colorado 2.8 Diesel Cluster v2 Mockup
- fault.c
- fdcan_obd.c
- stm32h7xx_hal_timebase_tim.c
- SNIFF page (STATE + ANALOG)
- test_alert.c
- test_fdcan_obd.c
- stdint
- Architecture
- lv_port_disp.c
- Сессия 2026-07-24, утро (~10:19–10:20) — расшифровка фото
- stm32h7xx_it.c
- main.c
- FDCAN1_IT0_IRQHandler
- stm32h7xx_hal_conf.h
- Свой одометр по GPS и расход топлива
- build.py
- main.c
- tim.c
- test_gps.c
- fault_record
- test_input.c
- GPS-скорость и замер разгона («свой Dragy»)
- Прогон 2026-07-21 (после ~5 мин поездки, снимок на парковке)
- test_st7735_status.c
- obd_init
- CLAUDE.md
- Findings
- main
- Экран: качество картинки и скорость
- fault_record
- build-hardware.md
- Плата-носитель 7×9 см: раскладка и провода
- In-vehicle install — from bench demo to a live car
- obd_pid_supported
- GM-enhanced DIDs (mode 22) for the Colorado 2.8 / E98
- string
- spi.c
- lv_port_disp_init
- Первая прошивка stage-c: чек-лист
- HAL_StatusTypeDef
- Статус исправлений (2026-10-09)
- HAL_GPIO_Init
- mock_lv_port_disp/lvgl.h
- st7735_status.c
- assembly_steps.py
- build_drive
- Dash — обзор проекта для ревью
- wiring_overview.py
- Oil pressure: where it comes from, and how to check it
- Сборка платы-носителя по шагам: что куда паять
- main
- carrier_board.py
- gps.c
- metric_key_t
- Жгут: провода от коробки к машине, экрану, кнопкам и GPS
- spi.c
- metric_state_t
- obd_data_t
- lv_display_get_screen_active
- SPI_HandleTypeDef
- SNIFF capture — selector / PRNDL search

## God Nodes (most connected - your core abstractions)
1. `obd_rx_poll()` - 89 edges
2. `st7735_status_init()` - 35 edges
3. `obd_poll_tick()` - 34 edges
4. `near()` - 27 edges
5. `AppMain_Run()` - 25 edges
6. `cluster_ui_refresh()` - 21 edges
7. `req_mode01()` - 20 edges
8. `nvlog_init()` - 18 edges
9. `nvlog_service()` - 18 edges
10. `req_oilp()` - 16 edges

## Surprising Connections (you probably didn't know these)
- `t_saturated_init_failure()` --calls--> `lv_port_disp_service()`  [INFERRED]
  tests/host/test_lv_port_disp.c → Core/Src/lv_port_disp.c
- `t_c7_egt_mask()` --calls--> `policy_shown()`  [INFERRED]
  tests/host/test_fdcan_obd.c → Core/Inc/cluster_policy.h
- `t_d02_oilp_ff_clears_value()` --calls--> `policy_shown()`  [INFERRED]
  tests/host/test_fdcan_obd.c → Core/Inc/cluster_policy.h
- `t_summary_needs_complete_data()` --calls--> `policy_shown()`  [INFERRED]
  tests/host/test_fdcan_obd.c → Core/Inc/cluster_policy.h
- `t_sniff_fail_closed()` --calls--> `policy_sniff_allowed()`  [INFERRED]
  tests/host/test_fdcan_obd.c → Core/Inc/cluster_policy.h

## Import Cycles
- None detected.

## Hyperedges (group relationships)
- **Three-Page Cluster Navigation (Drive/DPF/Diag)** — design_diesel_cluster_v2_3_drive_page, design_diesel_cluster_v2_3_dpf_page, design_diesel_cluster_v2_3_diag_page, design_diesel_cluster_v2_3_tab_switcher [EXTRACTED 1.00]
- **Glanceable Diesel-Health Signature Elements** — design_diesel_cluster_v2_3_systems_rail, design_diesel_cluster_v2_3_calibration_eyebrow, design_diesel_cluster_v2_3_state_colour_encoding, design_diesel_cluster_v2_3_ili9488_constraints [INFERRED 0.75]

## Communities (69 total, 12 thin omitted)

### Community 0 - "obd_rx_poll"
Cohesion: 0.08
Nodes (49): obd_rx_poll(), req_oilp(), egt_reply(), egt_request(), last_tx_is(), main(), near(), park() (+41 more)

### Community 1 - "Чистовая сборка: из макетки в устройство"
Cohesion: 0.06
Nodes (33): 1. Что не так с макетом, 2. Выбранная схема (вариант A) и что потом, 3. Схема узлов (вариант A), 4. Корпус и установка, 5. Порядок сборки и проверки, 6. Стоимость варианта A, 6a. Если бюджет позволяет (было в первой версии, ~$100–150), 6b. Заказ на LCSC (проверено по API LCSC 2026-10-11) (+25 more)

### Community 2 - "Error_Handler"
Cohesion: 0.12
Nodes (36): FDCAN_HandleTypeDef, cluster_app_init(), speed_cal_band(), speed_cal_factor(), speed_cal_gps(), speed_cal_obd(), speed_cal_reset(), steady() (+28 more)

### Community 3 - "cluster_ui.c"
Cohesion: 0.12
Nodes (32): apply_filter(), can_sniff_candidates(), can_sniff_feed(), can_sniff_filter_error(), can_sniff_fps(), can_sniff_id_count(), can_sniff_is_active(), can_sniff_movers() (+24 more)

### Community 4 - "test_lv_port_disp.c"
Cohesion: 0.05
Nodes (76): cs_high(), cs_low(), dc_cmd(), dc_data(), disp_flush_cb(), dma_wait(), HAL_SPI_ErrorCallback(), HAL_SPI_TxCpltCallback() (+68 more)

### Community 5 - "Colorado 2.8 Diesel Cluster v2 Mockup"
Cohesion: 0.22
Nodes (13): Calibration Eyebrow, Colorado 2.8 Diesel Cluster v2 Mockup, Holden Colorado RG 2.8 E98 ECM, DIAG Page, DPF Page, DPF Soot / Regeneration Monitoring, DRIVE Page, ECU Instrument Direction (+5 more)

### Community 6 - "fault.c"
Cohesion: 0.20
Nodes (10): assert_text(), fault_init(), fault_selftest(), fault_wdg_kick(), fault_wdg_run_mode(), fault_wdg_start_boot(), rec_set_streak(), wdg_set() (+2 more)

### Community 7 - "fdcan_obd.c"
Cohesion: 0.09
Nodes (41): obd_on_stamp(), obd_on_update(), arrived_in_time(), metric_key_t, obd_data_t, can_send(), decode_mode01(), decode_mode22() (+33 more)

### Community 8 - "stm32h7xx_hal_timebase_tim.c"
Cohesion: 0.33
Nodes (3): HAL_StatusTypeDef, HAL_InitTick(), stm32h7xx_hal_tim

### Community 9 - "SNIFF page (STATE + ANALOG)"
Cohesion: 0.10
Nodes (19): DIAG page, DPF page, DRIVE page, SNIFF page (STATE + ANALOG), Сессия 2026-07-23, утро (~10:10) — расшифровка фото, Фото 10 (628) — двигатель не запущен, Фото 11 (629), Фото 12 (630) — двигатель заглушен, WARN (+11 more)

### Community 10 - "test_alert.c"
Cohesion: 0.12
Nodes (39): alert, alert_cfg_t, alert_t, metric_key_t, obd_data_t, policy_complete(), policy_ecm_fresh(), policy_fresh() (+31 more)

### Community 11 - "test_fdcan_obd.c"
Cohesion: 0.32
Nodes (8): obd_pid_support(), obd_pid_supported(), pidsup_is_map(), obd_pid_support_t, ps_run(), t_pidsup_gives_up_unanswered(), t_pidsup_map_from_any_reply(), t_pidsup_reads_advertised_chain()

### Community 12 - "stdint"
Cohesion: 0.09
Nodes (10): metric_key_t, metric_state_t, metric_state(), fw_version, stdbool, stddef, stdint, lv_area_t (+2 more)

### Community 13 - "Architecture"
Cohesion: 0.13
Nodes (50): build_diag(), build_drive(), build_perf(), build_sniff(), build_stats(), build_strip(), build_trip(), cat() (+42 more)

### Community 15 - "Сессия 2026-07-24, утро (~10:19–10:20) — расшифровка фото"
Cohesion: 0.11
Nodes (17): 1. OIL P нестабильна — приоритет высокий, 2. DPF данные нестабильны, 3. DPF page WARN при dP=NaN, 4. BATT 11.9V — WARN alert, DIAG page, DPF page, DRIVE page, SNIFF page (+9 more)

### Community 17 - "main.c"
Cohesion: 0.25
Nodes (5): app_main, TIM_HandleTypeDef, HAL_TIM_PeriodElapsedCallback(), fault, main

### Community 18 - "FDCAN1_IT0_IRQHandler"
Cohesion: 0.13
Nodes (24): FDCAN1_IT0_IRQHandler(), obd_can_health(), obd_sel_fresh(), obd_health_t, obd_data_t, obd_is_fresh(), irq_mode(), t_boost_needs_baro_and_follows_map() (+16 more)

### Community 19 - "stm32h7xx_hal_conf.h"
Cohesion: 0.03
Nodes (61): stm32h7xx_hal_adc, stm32h7xx_hal_cec, stm32h7xx_hal_comp, stm32h7xx_hal_cordic, stm32h7xx_hal_cortex, stm32h7xx_hal_crc, stm32h7xx_hal_cryp, stm32h7xx_hal_dac (+53 more)

### Community 21 - "Свой одометр по GPS и расход топлива"
Cohesion: 0.18
Nodes (11): 1. Что показываем, 2. Пробег: откуда брать расстояние, 3. Расход топлива: откуда брать, 4. Хранение, 5. Экран и управление, 6. Прошивка: из чего состоит, 7. Что сделать первым, Источники (+3 more)

### Community 22 - "build.py"
Cohesion: 0.10
Nodes (52): nv_data_t, enter_next_sector(), erased(), error(), get32(), next_sector(), nv_crc32(), nv_decode() (+44 more)

### Community 23 - "main.c"
Cohesion: 0.14
Nodes (21): bus(), cap(), check(), diode(), esc(), module(), Generate docs/future/assembly/step-*.svg: the carrier board, one subsystem per s, Resistor between two points on one axis; label beside the body. (+13 more)

### Community 24 - "tim.c"
Cohesion: 0.43
Nodes (6): TIM_HandleTypeDef, HAL_TIM_MspPostInit(), HAL_TIM_PWM_MspDeInit(), HAL_TIM_PWM_MspInit(), MX_TIM1_Init(), tim

### Community 25 - "test_gps.c"
Cohesion: 0.06
Nodes (61): gps_state_t, enter(), gps_fix_fresh(), gps_init(), gps_poll(), gps_state(), gps_hw_getc(), gps_hw_init() (+53 more)

### Community 27 - "fault_record"
Cohesion: 0.29
Nodes (7): fault_assert_failed(), fault_record(), vApplicationStackOverflowHook(), MX_FREERTOS_Init(), HardFault_Handler(), fault_code_t, TaskHandle_t

### Community 28 - "test_input.c"
Cohesion: 0.22
Nodes (18): btn_out_t, btn_t, btn_step(), daynight_step(), daynight_t, input, exti_press(), hold_level() (+10 more)

### Community 29 - "GPS-скорость и замер разгона («свой Dragy»)"
Cohesion: 0.11
Nodes (18): 1. Зачем GPS, если есть OBD-скорость, 2. Аналоги, 3. Выбор GNSS-чипа, 4. Интеграция в текущую систему, 5. План прошивки и трудозатраты, 6. Подводные камни, 7. Резервное питание модуля: не нужно, 8. Железо: варианты и BOM (+10 more)

### Community 30 - "Прогон 2026-07-21 (после ~5 мин поездки, снимок на парковке)"
Cohesion: 0.20
Nodes (9): Анализ: что ответили новые enhanced-DID, Выводы / что дальше, КРИТИЧНО: скорость/обороты/rail/batt/load не читались, Прогон 2026-07-21 (после ~5 мин поездки, снимок на парковке), Расшифровка, Фото 1–4 — SNIFF (широковещательные кадры, мотор заглушён), Фото 5 — DIAG, Фото 6 — DPF (+1 more)

### Community 31 - "test_st7735_status.c"
Cohesion: 0.06
Nodes (85): lv_display_t, st7735_status_backlight(), st7735_status_init(), st7735_status_key_dbg(), st7735_status_note(), st7735_status_recoveries(), st7735_status_service(), st7735_status_set() (+77 more)

### Community 32 - "obd_init"
Cohesion: 0.10
Nodes (32): FDCAN_HandleTypeDef, HAL_StatusTypeDef, cfg_ok(), HAL_FDCAN_RxFifo0Callback(), obd_check_health(), obd_init(), rx_pump(), FDCAN_ProtocolStatusTypeDef (+24 more)

### Community 33 - "CLAUDE.md"
Cohesion: 0.22
Nodes (7): Architecture, Build / flash / debug, Git workflow: commit and push automatically, graphify, Hardware notes worth knowing before touching related code, Use graphify before and after non-trivial changes, What this is

### Community 34 - "Findings"
Cohesion: 0.20
Nodes (9): 1. OIL P (oil pressure) — candidate 0x1BA[3] is NOT oil pressure — REJECT, 2. SPEED — ~10 s lag + ~12 % high, 3. OIL temp — plausible magnitude, trend to verify, 4. Reads OK (no action), DIAG page (photos 17–18), DRIVE page readings (photos 19–26), Findings, Road-test session — 2026-08-23 (26 photos) (+1 more)

### Community 35 - "main"
Cohesion: 0.20
Nodes (17): concurrent_futures, hashlib, re, compile_one(), find_programmer(), find_toolchain(), flags_stamp(), flash() (+9 more)

### Community 36 - "Экран: качество картинки и скорость"
Cohesion: 0.10
Nodes (20): 0. Обновление 2026-10-22: «двоится», два глаза видят разные цвета, 1. Что сейчас и где теряется, 2. Время передачи кадра, 3. Этап 0: без покупок (сделать в любом случае), 4. Варианты железа, 5. Рекомендация, 6. Что купить, 6a. Что из этого есть на LCSC (проверено 2026-10-09) (+12 more)

### Community 37 - "fault_record"
Cohesion: 0.14
Nodes (13): Backlight control module, Boot / programming, CAN transceiver module, Core board, External modules (as purchased), Hardware reference — WeAct MiniSTM32H743, Main 4" display module, Onboard 0.96" ST7735 status display (SPI4) (+5 more)

### Community 39 - "Плата-носитель 7×9 см: раскладка и провода"
Cohesion: 0.25
Nodes (8): 1. Распиновка гребёнок WeAct, 2. Гнёзда под WeAct: хватит двух 1×40, 3. Клеммники 3.81 мм на сетке 2.54: гнуть выводы, 4. Что где стоит, 5. Провода, 6. Проверки до установки WeAct, 7. Высота в коробке 140×82×38, Плата-носитель 7×9 см: раскладка и провода

### Community 40 - "In-vehicle install — from bench demo to a live car"
Cohesion: 0.15
Nodes (12): 1. Powering the board in the car, 2. Connecting to the CAN bus (OBD-II port), 3. What the firmware sends, 4. Bring-up sequence, 5. Known gaps for a "complete" in-car build, Appendix: full connection & power map, ⚠️ Bus termination — the #1 thing to get right, In-vehicle install — from bench demo to a live car (+4 more)

### Community 41 - "obd_pid_supported"
Cohesion: 0.05
Nodes (71): can_sniff, cluster_app, cluster_ui, AppMain_Init(), AppMain_Run(), backlight_set(), bl_active(), bl_edit() (+63 more)

### Community 42 - "GM-enhanced DIDs (mode 22) for the Colorado 2.8 / E98"
Cohesion: 0.29
Nodes (7): Added, awaiting on-car confirmation, Being probed on the car (standard diesel PIDs), Confirmed on this vehicle, GM-enhanced DIDs (mode 22) for the Colorado 2.8 / E98, Oil pressure — mode 22 confirmed absent on this E98, Oil pressure via passive CAN broadcast (superseded), Sources

### Community 43 - "string"
Cohesion: 0.27
Nodes (13): a(), box(), conn(), dot(), esc(), net(), Generate docs/future/wiring-overview.svg: what connects where, car -> box -> dis, Net tag: name drawn in a small flag, wire end at (x, y). (+5 more)

### Community 44 - "spi.c"
Cohesion: 0.40
Nodes (5): SPI_HandleTypeDef, HAL_SPI_MspDeInit(), HAL_SPI_MspInit(), MX_SPI2_Init(), spi

### Community 45 - "lv_port_disp_init"
Cohesion: 0.33
Nodes (6): Check on the car, DIAG, bottom readout (line 3), How the firmware asks (`fdcan_obd.c`, "oil pressure"), Oil pressure: where it comes from, and how to check it, The source, What the old candidates were

### Community 46 - "Первая прошивка stage-c: чек-лист"
Cohesion: 0.22
Nodes (9): 0. Какой образ шить, 1. Стол сейчас: WeAct + 4" на макетке, KEY, без машины и GPS, 2. Образ DEMO: цвета и тревоги без машины, 3. Проверка сбросов (FAULT_TEST), 4. Когда придёт GPS (ATGM336H), 5. Когда будет плата-носитель, 6. Первый раз в машине, 7. Потом (+1 more)

### Community 47 - "HAL_StatusTypeDef"
Cohesion: 0.40
Nodes (4): MX_GPIO_Init(), main(), MPU_Config(), gpio

### Community 48 - "Статус исправлений (2026-10-09)"
Cohesion: 0.50
Nodes (5): metric_key_t, put(), sel(), t_sniff_fail_closed(), t_summary_needs_complete_data()

### Community 51 - "st7735_status.c"
Cohesion: 0.10
Nodes (26): obd_poll_tick(), req_mode01(), req_mode22(), cap_scenario(), t_c3_01_cf_after_cap_rejected(), t_c3_01_cf_before_cap_completes(), t_d03_nrc78_total_cap(), t_d03_nrc78_within_cap_completes() (+18 more)

### Community 52 - "assembly_steps.py"
Cohesion: 0.21
Nodes (8): pymupdf, sys, board(), esc(), prow(), Generate docs/future/carrier-template.pdf: the 7x9 carrier at 1:1 on A4, to prin, Holes, labels, sockets, blocks and zones; mirror = view from the solder side., Sheet

### Community 54 - "Dash — обзор проекта для ревью"
Cohesion: 0.07
Nodes (30): 1. Что это, 2. Что работает в машине (по заметкам с выездов), 3. Архитектура, 4. Сборка, прошивка, инструменты, 5. Находки ревью, 6. Что сделано хорошо, 7. Устаревшая документация (расходится с кодом), 8. Вопросы автору (+22 more)

### Community 55 - "wiring_overview.py"
Cohesion: 0.25
Nodes (8): argparse, importlib_util, os, pathlib, shutil, subprocess, find_cc(), main()

### Community 57 - "Сборка платы-носителя по шагам: что куда паять"
Cohesion: 0.20
Nodes (10): После всех шагов, Сборка платы-носителя по шагам: что куда паять, Шаг 1. Питание, Шаг 2. Шины, Шаг 3. Экран (X5), Шаг 4. Подсветка (2N7000 + Si4599), Шаг 5. CAN (X2), Шаг 6. GPS (X3) (+2 more)

### Community 58 - "main"
Cohesion: 0.47
Nodes (8): a(), block(), hx(), hy(), part(), pin_xy(), prow(), Generate docs/future/carrier-board.svg — top view of the 7x9 cm carrier.  Usag

### Community 59 - "carrier_board.py"
Cohesion: 0.26
Nodes (20): perf_t, cross_t(), perf_clear(), perf_elapsed(), perf_gps(), perf_init(), result(), rolling() (+12 more)

### Community 60 - "gps.c"
Cohesion: 0.44
Nodes (8): a(), box(), cable(), esc(), plug(), Generate docs/future/harness.svg: the harness as a tree, box -> car / display /, A bundle: thin coloured lines side by side, each segment offset across its own d, text()

### Community 62 - "Жгут: провода от коробки к машине, экрану, кнопкам и GPS"
Cohesion: 0.25
Nodes (8): X1 — питание, X2 — CAN, X3 — GPS, X4 — кнопки и габариты, X5 — экран, Жгут: провода от коробки к машине, экрану, кнопкам и GPS, Общие правила, Перед первым включением в машине

### Community 63 - "spi.c"
Cohesion: 0.32
Nodes (7): FDCAN_HandleTypeDef, HAL_FDCAN_MspDeInit(), HAL_FDCAN_MspInit(), MX_FDCAN1_Init(), Error_Handler(), SystemClock_Config(), fdcan

### Community 68 - "SNIFF capture — selector / PRNDL search"
Cohesion: 0.33
Nodes (6): Analysis — distinct value set per interesting byte, Conclusion, Next, Run 1  (design/Photos/1/, frames 023–035), Run 2  (design/Photos/2/, frames 036–052), SNIFF capture — selector / PRNDL search

## Knowledge Gaps
- **207 isolated node(s):** `What this is`, `Git workflow: commit and push automatically`, `Build / flash / debug`, `Architecture`, `Use graphify before and after non-trivial changes` (+202 more)
  These have ≤1 connection - possible missing edges or undocumented components.
- **12 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `AppMain_Run()` connect `obd_pid_supported` to `test_lv_port_disp.c`, `fault.c`, `test_alert.c`, `test_gps.c`, `test_input.c`, `test_st7735_status.c`?**
  _High betweenness centrality (0.020) - this node is a cross-community bridge._
- **Why does `obd_rx_poll()` connect `obd_rx_poll` to `obd_init`, `cluster_ui.c`, `fdcan_obd.c`, `obd_pid_supported`, `test_alert.c`, `test_fdcan_obd.c`, `Статус исправлений (2026-10-09)`, `FDCAN1_IT0_IRQHandler`, `st7735_status.c`?**
  _High betweenness centrality (0.010) - this node is a cross-community bridge._
- **Why does `AppMain_Init()` connect `obd_pid_supported` to `Error_Handler`, `test_lv_port_disp.c`, `fault.c`, `test_gps.c`, `test_st7735_status.c`?**
  _High betweenness centrality (0.008) - this node is a cross-community bridge._
- **Are the 75 inferred relationships involving `obd_rx_poll()` (e.g. with `cluster_app_run()` and `can_sniff_feed()`) actually correct?**
  _`obd_rx_poll()` has 75 INFERRED edges - model-reasoned connections that need verification._
- **Are the 33 inferred relationships involving `st7735_status_init()` (e.g. with `AppMain_Init()` and `lv_color_black()`) actually correct?**
  _`st7735_status_init()` has 33 INFERRED edges - model-reasoned connections that need verification._
- **Are the 27 inferred relationships involving `obd_poll_tick()` (e.g. with `cluster_app_run()` and `can_sniff_is_active()`) actually correct?**
  _`obd_poll_tick()` has 27 INFERRED edges - model-reasoned connections that need verification._
- **What connects `What this is`, `Git workflow: commit and push automatically`, `Build / flash / debug` to the rest of the system?**
  _207 weakly-connected nodes found - possible documentation gaps or missing edges._