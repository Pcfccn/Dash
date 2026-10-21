# Graph Report - Dash  (2026-10-21)

## Corpus Check
- 97 files · ~834,959 words
- Verdict: corpus is large enough that graph structure adds value.
- Unclassified: 9 file(s) not represented in the graph (top: (none) 5, .ld 2, .ioc 1)

## Summary
- 1228 nodes · 2731 edges · 66 communities (61 shown, 5 thin omitted)
- Extraction: 72% EXTRACTED · 28% INFERRED · 0% AMBIGUOUS · INFERRED: 760 edges (avg confidence: 0.87)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `6cfa050b`
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
- obd_poll_tick
- FDCAN1_IT0_IRQHandler
- stm32h7xx_hal_conf.h
- Свой одометр по GPS и расход топлива
- build.py
- main.c
- tim.c
- test_gps.c
- test_can_sniff.c
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
- SNIFF capture — selector / PRNDL search
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
- carrier_template.py
- Сборка платы-носителя по шагам: что куда паять
- host_test.py
- carrier_board.py
- Sheet
- harness_tree.py
- Жгут: провода от коробки к машине, экрану, кнопкам и GPS
- spi.c
- HAL_StatusTypeDef
- HAL_GPIO_Init

## God Nodes (most connected - your core abstractions)
1. `obd_rx_poll()` - 94 edges
2. `cluster_ui_refresh()` - 50 edges
3. `obd_poll_tick()` - 39 edges
4. `Architecture` - 37 edges
5. `st7735_status_init()` - 36 edges
6. `near()` - 27 edges
7. `AppMain_Run()` - 26 edges
8. `req_mode01()` - 21 edges
9. `build_drive()` - 20 edges
10. `alerts_state()` - 18 edges

## Surprising Connections (you probably didn't know these)
- `5. Known gaps for a "complete" in-car build` --references--> `backlight_set()`  [INFERRED]
  docs/vehicle-install.md → Core/Src/app_main.c
- `1. Стол сейчас: WeAct + 4" на макетке, KEY, без машины и GPS` --references--> `AppMain_Init()`  [INFERRED]
  docs/bench-first-flash.md → Core/Src/app_main.c
- `Подсветка: драйвер затвора` --references--> `AppMain_Init()`  [INFERRED]
  docs/future/build-hardware.md → Core/Src/app_main.c
- `Низкий приоритет и гигиена` --references--> `fmt()`  [INFERRED]
  docs/REVIEW.md → Core/Src/cluster_ui.c
- `Канонический аудит Stage C (2026-10-10) — статус по пунктам` --references--> `cluster_ui_set_page()`  [INFERRED]
  docs/REVIEW.md → Core/Src/cluster_ui.c

## Import Cycles
- None detected.

## Hyperedges (group relationships)
- **Three-Page Cluster Navigation (Drive/DPF/Diag)** — design_diesel_cluster_v2_3_drive_page, design_diesel_cluster_v2_3_dpf_page, design_diesel_cluster_v2_3_diag_page, design_diesel_cluster_v2_3_tab_switcher [EXTRACTED 1.00]
- **Glanceable Diesel-Health Signature Elements** — design_diesel_cluster_v2_3_systems_rail, design_diesel_cluster_v2_3_calibration_eyebrow, design_diesel_cluster_v2_3_state_colour_encoding, design_diesel_cluster_v2_3_ili9488_constraints [INFERRED 0.75]

## Communities (66 total, 5 thin omitted)

### Community 0 - "obd_rx_poll"
Cohesion: 0.08
Nodes (33): can_sniff_feed(), obd_rx_poll(), obd_sel_fresh(), past_txn_cap(), req_mode01(), rxq_pop(), rxq_t, metric_key_t (+25 more)

### Community 1 - "Чистовая сборка: из макетки в устройство"
Cohesion: 0.06
Nodes (33): 1. Что не так с макетом, 2. Выбранная схема (вариант A) и что потом, 3. Схема узлов (вариант A), 4. Корпус и установка, 5. Порядок сборки и проверки, 6. Стоимость варианта A, 6a. Если бюджет позволяет (было в первой версии, ~$100–150), 6b. Заказ на LCSC (проверено по API LCSC 2026-10-11) (+25 more)

### Community 2 - "Error_Handler"
Cohesion: 0.24
Nodes (9): FDCAN_HandleTypeDef, HAL_FDCAN_MspDeInit(), HAL_FDCAN_MspInit(), MX_FDCAN1_Init(), HAL_StatusTypeDef, cfg_ok(), Error_Handler(), SystemClock_Config() (+1 more)

### Community 3 - "cluster_ui.c"
Cohesion: 0.17
Nodes (30): Hardware notes worth knowing before touching related code, lv_color_t, metric_key_t, metric_state_t, obd_data_t, cluster_ui_next_page(), cluster_ui_prev_page(), cluster_ui_refresh() (+22 more)

### Community 4 - "test_lv_port_disp.c"
Cohesion: 0.19
Nodes (19): lv_port_disp_service(), IRQn_Type, dma_done(), flush(), HAL_NVIC_EnableIRQ(), HAL_NVIC_SetPriority(), t_boot_errors_then_good_dma(), t_dma_refused_fallback_fails() (+11 more)

### Community 5 - "Colorado 2.8 Diesel Cluster v2 Mockup"
Cohesion: 0.22
Nodes (13): Calibration Eyebrow, Colorado 2.8 Diesel Cluster v2 Mockup, Holden Colorado RG 2.8 E98 ECM, DIAG Page, DPF Page, DPF Soot / Regeneration Monitoring, DRIVE Page, ECU Instrument Direction (+5 more)

### Community 6 - "fault.c"
Cohesion: 0.20
Nodes (13): assert_text(), fault_init(), fault_reset_abnormal(), fault_reset_streak(), fault_reset_text(), fault_selftest(), fault_wdg_kick(), fault_wdg_run_mode() (+5 more)

### Community 7 - "fdcan_obd.c"
Cohesion: 0.12
Nodes (36): obd_on_stamp(), obd_on_update(), arrived_in_time(), metric_key_t, obd_data_t, can_send(), decode_mode01(), decode_mode22() (+28 more)

### Community 8 - "stm32h7xx_hal_timebase_tim.c"
Cohesion: 0.33
Nodes (3): HAL_StatusTypeDef, HAL_InitTick(), stm32h7xx_hal_tim

### Community 9 - "SNIFF page (STATE + ANALOG)"
Cohesion: 0.10
Nodes (19): DIAG page, DPF page, DRIVE page, SNIFF page (STATE + ANALOG), Сессия 2026-07-23, утро (~10:10) — расшифровка фото, Фото 10 (628) — двигатель не запущен, Фото 11 (629), Фото 12 (630) — двигатель заглушен, WARN (+11 more)

### Community 10 - "test_alert.c"
Cohesion: 0.18
Nodes (25): alert, alert_cfg_t, alert_t, alert_reset(), alert_step(), alerts_engine_running(), alerts_state(), metric_key_t (+17 more)

### Community 11 - "test_fdcan_obd.c"
Cohesion: 0.08
Nodes (39): IRQn_Type, metric_key_t, cap_scenario(), egt_reply(), egt_request(), HAL_NVIC_DisableIRQ(), HAL_NVIC_EnableIRQ(), HAL_NVIC_SetPriority() (+31 more)

### Community 12 - "stdint"
Cohesion: 0.09
Nodes (8): metric_key_t, metric_state_t, metric_state(), fw_version, stdbool, stddef, stdint, stm32h7xx_hal

### Community 13 - "Architecture"
Cohesion: 0.07
Nodes (55): can_sniff, Architecture, cluster_app, cluster_ui, metric_key_t, obd_data_t, policy_complete(), policy_ecm_fresh() (+47 more)

### Community 14 - "lv_port_disp.c"
Cohesion: 0.18
Nodes (23): lv_area_t, lv_display_t, cs_high(), cs_low(), dc_cmd(), dc_data(), disp_flush_cb(), dma_wait() (+15 more)

### Community 15 - "Сессия 2026-07-24, утро (~10:19–10:20) — расшифровка фото"
Cohesion: 0.11
Nodes (17): 1. OIL P нестабильна — приоритет высокий, 2. DPF данные нестабильны, 3. DPF page WARN при dP=NaN, 4. BATT 11.9V — WARN alert, DIAG page, DPF page, DRIVE page, SNIFF page (+9 more)

### Community 17 - "obd_poll_tick"
Cohesion: 0.08
Nodes (35): can_sniff_is_active(), obd_poll_tick(), oilp_failed(), pidsup_next(), req_mode22(), req_oilp(), req_pidsup(), txn_failed() (+27 more)

### Community 18 - "FDCAN1_IT0_IRQHandler"
Cohesion: 0.25
Nodes (14): FDCAN1_IT0_IRQHandler(), obd_can_health(), obd_health_t, irq_mode(), t_d05_arrival_check_across_wrap(), t_d05_late_arrival_is_not_an_answer(), t_d06_sniff_gets_rx_time(), t_delayed_frame_across_wrap() (+6 more)

### Community 19 - "stm32h7xx_hal_conf.h"
Cohesion: 0.03
Nodes (61): stm32h7xx_hal_adc, stm32h7xx_hal_cec, stm32h7xx_hal_comp, stm32h7xx_hal_cordic, stm32h7xx_hal_cortex, stm32h7xx_hal_crc, stm32h7xx_hal_cryp, stm32h7xx_hal_dac (+53 more)

### Community 21 - "Свой одометр по GPS и расход топлива"
Cohesion: 0.18
Nodes (11): 1. Что показываем, 2. Пробег: откуда брать расстояние, 3. Расход топлива: откуда брать, 4. Хранение, 5. Экран и управление, 6. Прошивка: из чего состоит, 7. Что сделать первым, Источники (+3 more)

### Community 22 - "build.py"
Cohesion: 0.20
Nodes (15): concurrent_futures, hashlib, re, compile_one(), find_toolchain(), flags_stamp(), main(), obj_for() (+7 more)

### Community 23 - "main.c"
Cohesion: 0.24
Nodes (5): app_main, TIM_HandleTypeDef, HAL_TIM_PeriodElapsedCallback(), fault, main

### Community 24 - "tim.c"
Cohesion: 0.43
Nodes (6): TIM_HandleTypeDef, HAL_TIM_MspPostInit(), HAL_TIM_PWM_MspDeInit(), HAL_TIM_PWM_MspInit(), MX_TIM1_Init(), tim

### Community 25 - "test_gps.c"
Cohesion: 0.06
Nodes (61): enter(), gps_fix_fresh(), gps_init(), gps_poll(), gps_state(), gps_hw_getc(), gps_hw_init(), gps_hw_line_ms() (+53 more)

### Community 27 - "test_can_sniff.c"
Cohesion: 0.12
Nodes (31): apply_filter(), can_sniff_candidates(), can_sniff_filter_error(), can_sniff_fps(), can_sniff_id_count(), can_sniff_movers(), can_sniff_reset(), can_sniff_restore_pending() (+23 more)

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
Cohesion: 0.10
Nodes (41): st7735_status_backlight(), st7735_status_init(), st7735_status_key_dbg(), st7735_status_note(), st7735_status_set(), lv_lcd_flag_t, lv_st7735_send_cmd_cb_t, lv_st7735_send_color_cb_t (+33 more)

### Community 32 - "obd_init"
Cohesion: 0.13
Nodes (26): FDCAN_HandleTypeDef, HAL_FDCAN_RxFifo0Callback(), obd_check_health(), obd_init(), rx_pump(), FDCAN_ProtocolStatusTypeDef, FDCAN_RxHeaderTypeDef, FDCAN_TxHeaderTypeDef (+18 more)

### Community 33 - "CLAUDE.md"
Cohesion: 0.29
Nodes (5): Build / flash / debug, Git workflow: commit and push automatically, graphify, Use graphify before and after non-trivial changes, What this is

### Community 34 - "Findings"
Cohesion: 0.20
Nodes (9): 1. OIL P (oil pressure) — candidate 0x1BA[3] is NOT oil pressure — REJECT, 2. SPEED — ~10 s lag + ~12 % high, 3. OIL temp — plausible magnitude, trend to verify, 4. Reads OK (no action), DIAG page (photos 17–18), DRIVE page readings (photos 19–26), Findings, Road-test session — 2026-08-23 (26 photos) (+1 more)

### Community 35 - "main"
Cohesion: 0.40
Nodes (4): MX_GPIO_Init(), main(), MPU_Config(), gpio

### Community 36 - "Экран: качество картинки и скорость"
Cohesion: 0.13
Nodes (15): 1. Что сейчас и где теряется, 2. Время передачи кадра, 3. Этап 0: без покупок (сделать в любом случае), 4. Варианты железа, 5. Рекомендация, 6. Что купить, 6a. Что из этого есть на LCSC (проверено 2026-10-09), 7. Подводные камни (+7 more)

### Community 37 - "fault_record"
Cohesion: 0.29
Nodes (7): fault_assert_failed(), fault_record(), vApplicationStackOverflowHook(), MX_FREERTOS_Init(), HardFault_Handler(), fault_code_t, TaskHandle_t

### Community 39 - "Плата-носитель 7×9 см: раскладка и провода"
Cohesion: 0.25
Nodes (8): 1. Распиновка гребёнок WeAct, 2. Гнёзда под WeAct: хватит двух 1×40, 3. Клеммники 3.81 мм на сетке 2.54: гнуть выводы, 4. Что где стоит, 5. Провода, 6. Проверки до установки WeAct, 7. Высота в коробке 140×82×38, Плата-носитель 7×9 см: раскладка и провода

### Community 40 - "In-vehicle install — from bench demo to a live car"
Cohesion: 0.18
Nodes (11): 1. Powering the board in the car, 2. Connecting to the CAN bus (OBD-II port), 3. What the firmware sends, 4. Bring-up sequence, 5. Known gaps for a "complete" in-car build, Appendix: full connection & power map, ⚠️ Bus termination — the #1 thing to get right, In-vehicle install — from bench demo to a live car (+3 more)

### Community 41 - "obd_pid_supported"
Cohesion: 0.38
Nodes (7): obd_pid_support(), obd_pid_supported(), obd_pid_support_t, ps_run(), t_pidsup_gives_up_unanswered(), t_pidsup_map_from_any_reply(), t_pidsup_reads_advertised_chain()

### Community 42 - "GM-enhanced DIDs (mode 22) for the Colorado 2.8 / E98"
Cohesion: 0.13
Nodes (13): Added, awaiting on-car confirmation, Being probed on the car (standard diesel PIDs), Confirmed on this vehicle, GM-enhanced DIDs (mode 22) for the Colorado 2.8 / E98, Oil pressure — mode 22 confirmed absent on this E98, Oil pressure via passive CAN broadcast (superseded), Sources, Check on the car (+5 more)

### Community 43 - "string"
Cohesion: 0.13
Nodes (36): ms_txt(), refresh_stats(), speed_cal_band(), speed_cal_factor(), speed_cal_gps(), speed_cal_obd(), speed_cal_reset(), steady() (+28 more)

### Community 44 - "SNIFF capture — selector / PRNDL search"
Cohesion: 0.29
Nodes (6): Analysis — distinct value set per interesting byte, Conclusion, Next, Run 1  (design/Photos/1/, frames 023–035), Run 2  (design/Photos/2/, frames 036–052), SNIFF capture — selector / PRNDL search

### Community 45 - "lv_port_disp_init"
Cohesion: 0.13
Nodes (19): lv_port_disp_init(), lv_display_flush_cb_t, lv_tick_get_cb_t, lv_display_t, lv_obj_t, lv_display_create(), lv_display_flush_ready(), lv_display_get_screen_active() (+11 more)

### Community 46 - "Первая прошивка stage-c: чек-лист"
Cohesion: 0.22
Nodes (9): 0. Какой образ шить, 1. Стол сейчас: WeAct + 4" на макетке, KEY, без машины и GPS, 2. Образ DEMO: цвета и тревоги без машины, 3. Проверка сбросов (FAULT_TEST), 4. Когда придёт GPS (ATGM336H), 5. Когда будет плата-носитель, 6. Первый раз в машине, 7. Потом (+1 more)

### Community 47 - "HAL_StatusTypeDef"
Cohesion: 0.21
Nodes (12): DMA1_Stream0_IRQHandler(), SPI2_IRQHandler(), DMA_HandleTypeDef, HAL_StatusTypeDef, SPI_HandleTypeDef, HAL_DMA_Init(), HAL_DMA_IRQHandler(), HAL_SPI_Abort() (+4 more)

### Community 48 - "Статус исправлений (2026-10-09)"
Cohesion: 0.29
Nodes (7): SPI_HandleTypeDef, HAL_SPI_TxCpltCallback(), Канонический аудит Stage C (2026-10-10) — статус по пунктам, Полный аудит 2026-10-10 — статус по пунктам, Ревью Stage C4 (2026-10-11) — статус, Ревью Stage C6 (2026-10-11) — статус, Статус исправлений (2026-10-09)

### Community 49 - "HAL_GPIO_Init"
Cohesion: 0.40
Nodes (5): GPIO_InitTypeDef, GPIO_PinState, GPIO_TypeDef, HAL_GPIO_Init(), HAL_GPIO_WritePin()

### Community 50 - "mock_lv_port_disp/lvgl.h"
Cohesion: 0.67
Nodes (3): lv_area_t, lv_area_get_height(), lv_area_get_width()

### Community 51 - "st7735_status.c"
Cohesion: 0.13
Nodes (29): btn_snapshot(), cat(), lv_display_t, st7735_status_recoveries(), st7735_status_service(), st7735_status_spi_errors(), st_broken(), st_cs_high() (+21 more)

### Community 52 - "assembly_steps.py"
Cohesion: 0.09
Nodes (18): cap(), diode(), esc(), module(), Generate docs/future/assembly/step-*.svg: the carrier board, one subsystem per…, Resistor between two points on one axis; label beside the body., Ceramic capacitor (no polarity), vertical., Vertical diode body with its band (cathode). (+10 more)

### Community 53 - "build_drive"
Cohesion: 0.38
Nodes (24): build_diag(), build_drive(), build_sniff(), build_stats(), build_strip(), lv_font_t, lv_obj_t, cluster_ui_build() (+16 more)

### Community 54 - "Dash — обзор проекта для ревью"
Cohesion: 0.11
Nodes (18): 1. Что это, 2. Что работает в машине (по заметкам с выездов), 3. Архитектура, 4. Сборка, прошивка, инструменты, 5. Находки ревью, 6. Что сделано хорошо, 7. Устаревшая документация (расходится с кодом), 8. Вопросы автору (+10 more)

### Community 55 - "wiring_overview.py"
Cohesion: 0.27
Nodes (13): a(), box(), conn(), dot(), esc(), net(), Generate docs/future/wiring-overview.svg: what connects where, car -> box ->…, Net tag: name drawn in a small flag, wire end at (x, y). (+5 more)

### Community 56 - "carrier_template.py"
Cohesion: 0.20
Nodes (8): pathlib, pymupdf, sys, board(), esc(), prow(), Generate docs/future/carrier-template.pdf: the 7x9 carrier at 1:1 on A4, to…, Holes, labels, sockets, blocks and zones; mirror = view from the solder side.

### Community 57 - "Сборка платы-носителя по шагам: что куда паять"
Cohesion: 0.20
Nodes (10): После всех шагов, Сборка платы-носителя по шагам: что куда паять, Шаг 1. Питание, Шаг 2. Шины, Шаг 3. Экран (X5), Шаг 4. Подсветка (2N7000 + Si4599), Шаг 5. CAN (X2), Шаг 6. GPS (X3) (+2 more)

### Community 58 - "host_test.py"
Cohesion: 0.25
Nodes (8): argparse, importlib_util, os, shutil, subprocess, find_cc(), main(), Build and run the host-side unit tests in tests/host. python tools/host_test.py…

### Community 59 - "carrier_board.py"
Cohesion: 0.47
Nodes (8): a(), block(), hx(), hy(), part(), pin_xy(), prow(), Generate docs/future/carrier-board.svg — top view of the 7x9 cm carrier. Usage…

### Community 61 - "harness_tree.py"
Cohesion: 0.44
Nodes (8): a(), box(), cable(), esc(), plug(), Generate docs/future/harness.svg: the harness as a tree, box -> car / display /…, A bundle: thin coloured lines side by side, each segment offset across its own…, text()

### Community 62 - "Жгут: провода от коробки к машине, экрану, кнопкам и GPS"
Cohesion: 0.25
Nodes (8): X1 — питание, X2 — CAN, X3 — GPS, X4 — кнопки и габариты, X5 — экран, Жгут: провода от коробки к машине, экрану, кнопкам и GPS, Общие правила, Перед первым включением в машине

### Community 63 - "spi.c"
Cohesion: 0.40
Nodes (5): SPI_HandleTypeDef, HAL_SPI_MspDeInit(), HAL_SPI_MspInit(), MX_SPI2_Init(), spi

### Community 64 - "HAL_StatusTypeDef"
Cohesion: 0.53
Nodes (6): HAL_StatusTypeDef, SPI_HandleTypeDef, HAL_SPI_Abort(), HAL_SPI_DeInit(), HAL_SPI_Init(), HAL_SPI_Transmit()

### Community 65 - "HAL_GPIO_Init"
Cohesion: 0.40
Nodes (5): GPIO_InitTypeDef, GPIO_PinState, GPIO_TypeDef, HAL_GPIO_Init(), HAL_GPIO_WritePin()

## Knowledge Gaps
- **174 isolated node(s):** `What this is`, `Git workflow: commit and push automatically`, `Use graphify before and after non-trivial changes`, `graphify`, `Фото 7 (625) — двигатель работает, RPM=1056` (+169 more)
  These have ≤1 connection - possible missing edges or undocumented components. (Counts symbols only; 378 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **5 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `Architecture` connect `Architecture` to `obd_rx_poll`, `Error_Handler`, `cluster_ui.c`, `fault.c`, `fdcan_obd.c`, `test_alert.c`, `stdint`, `obd_poll_tick`, `FDCAN1_IT0_IRQHandler`, `test_gps.c`, `test_can_sniff.c`, `test_input.c`, `test_st7735_status.c`, `obd_init`, `CLAUDE.md`, `fault_record`, `obd_pid_supported`, `lv_port_disp_init`, `build_drive`?**
  _High betweenness centrality (0.089) - this node is a cross-community bridge._
- **Why does `cluster_ui_refresh()` connect `cluster_ui.c` to `obd_rx_poll`, `Экран: качество картинки и скорость`, `fdcan_obd.c`, `obd_pid_supported`, `test_alert.c`, `string`, `Architecture`, `lv_port_disp.c`, `obd_poll_tick`, `FDCAN1_IT0_IRQHandler`, `st7735_status.c`, `build_drive`, `Dash — обзор проекта для ревью`, `test_gps.c`, `test_can_sniff.c`, `test_st7735_status.c`?**
  _High betweenness centrality (0.077) - this node is a cross-community bridge._
- **Why does `obd_rx_poll()` connect `obd_rx_poll` to `obd_init`, `fdcan_obd.c`, `obd_pid_supported`, `test_fdcan_obd.c`, `Architecture`, `obd_poll_tick`, `FDCAN1_IT0_IRQHandler`, `Dash — обзор проекта для ревью`, `GPS-скорость и замер разгона («свой Dragy»)`?**
  _High betweenness centrality (0.065) - this node is a cross-community bridge._
- **Are the 80 inferred relationships involving `obd_rx_poll()` (e.g. with `Architecture` and `cluster_app_run()`) actually correct?**
  _`obd_rx_poll()` has 80 INFERRED edges - model-reasoned connections that need verification._
- **Are the 35 inferred relationships involving `cluster_ui_refresh()` (e.g. with `Architecture` and `Hardware notes worth knowing before touching related code`) actually correct?**
  _`cluster_ui_refresh()` has 35 INFERRED edges - model-reasoned connections that need verification._
- **Are the 32 inferred relationships involving `obd_poll_tick()` (e.g. with `Architecture` and `cluster_app_run()`) actually correct?**
  _`obd_poll_tick()` has 32 INFERRED edges - model-reasoned connections that need verification._
- **Are the 36 inferred relationships involving `Architecture` (e.g. with `metric_state()` and `policy_complete()`) actually correct?**
  _`Architecture` has 36 INFERRED edges - model-reasoned connections that need verification._